#include <CLed.h>
#include <CLight.h>
#include <CWifi.h>
#include <config.h>
#include <ctime>

void OnLightValueChanged(void *pObject, CConfigKeyBase *pKey) {
  static_cast<CLight_1Ch *>(pObject)->OnLightValueChanged(pKey);
}
void OnEnableChanged(void *pObject, CConfigKeyBase *pKey) {
  static_cast<CLight_1Ch *>(pObject)->OnEnableChanged(pKey);
}

CLight_1Ch::CLight_1Ch()
    : CControl("CLight"), m_eControlState(eInit),
      m_eLightControlMode(eAutomatic), m_dValueWhiteP(0.0),
      m_dValueWhiteP_(0.0) {

  pinMode(PIN_WHITE, OUTPUT);
  digitalWrite(PIN_WHITE, LOW);

  analogWriteFreq(25000);
  analogWriteRange(PWM_RANGE);

  m_pWhiteValMinP =
      CreateConfigKeyIntSlider("Light", "WhiteValueMin", 0, 0, 100);
  m_pWhiteValMinP->SetOnChangedCallback(::OnLightValueChanged, this);

  m_pWhiteValMaxP =
      CreateConfigKeyIntSlider("Light", "WhiteValueMax", 100, 0, 100);
  m_pWhiteValMaxP->SetOnChangedCallback(::OnLightValueChanged, this);

  m_pWhiteValMoonP =
      CreateConfigKeyIntSlider("Light", "WhiteValueMoon", 20, 0, 100);
  m_pWhiteValMoonP->SetOnChangedCallback(::OnLightValueChanged, this);

  //! DAYLIGHT CONTROL
  m_pFadeTimeSec = CreateConfigKeyTimeString("Timing", "FadeTime", "5:00");
  m_pTimeMoonOn = CreateConfigKeyTimeString("Timing", "MoonOn", "07:00");
  m_pTimeWhiteOn = CreateConfigKeyTimeString("Timing", "WhiteOn", "08:00");
  m_pTimeWhiteOff = CreateConfigKeyTimeString("Timing", "WhiteOff", "18:00");
  m_pTimeMoonOff = CreateConfigKeyTimeString("Timing", "MoonOff", "22:00");

  //! CLOUD MODE
  m_pCloudEnabled = CreateConfigKey<bool>("Clouds", "Enabled", true);
  m_pWhiteValCloudP =
      CreateConfigKeyIntSlider("Clouds", "WhiteValueCloud", 80, 0, 100);
  m_pCloudChance = CreateConfigKey<int>("Clouds", "Chance", 2);
  m_pCloudeCycle = CreateConfigKeyTimeString("Clouds", "Cycle", "05:00");

//! NOON OFF MODE
#if WITH_NOONMODE == 1
  m_pNoonOff = CreateConfigKey<bool>("NoonMode", "Enabled", true);
  m_pNoonOff->SetOnChangedCallback(::OnEnableChanged, this);
  m_pTimeNoonOff = CreateConfigKeyTimeString("NoonMode", "NoonOff", "11:00");
  m_pTimeNoonOn = CreateConfigKeyTimeString("NoonMode", "NoonOn", "15:00");
#endif

  m_pMorning_MoonOn =
      new CState("Morning_MoonOn", m_pTimeMoonOn, m_pWhiteValMoonP);
  m_pMorning_WhiteOn =
      new CState("Morning_WhiteOn", m_pTimeWhiteOn, m_pWhiteValMaxP, true);
  m_pEvening_WhiteOff =
      new CState("Evening_WhiteOff", m_pTimeWhiteOff, m_pWhiteValMoonP);
  m_pEvening_MoonOff =
      new CState("Evening_MoonOff", m_pTimeMoonOff, m_pWhiteValMinP);

  m_pMorning_MoonOn->SetNext(m_pMorning_WhiteOn);
  m_pMorning_WhiteOn->SetNext(m_pEvening_WhiteOff);
  m_pEvening_WhiteOff->SetNext(m_pEvening_MoonOff);

  LogCStateChain();
}

bool CLight_1Ch::setup() {
  m_pMqtt_LightValueWhite = CreateMqttValue("LightValue.White", "0");
  m_pMqtt_TimeToStateChangeS = CreateMqttValue("TimeToStateChange", "0");
  m_pMqtt_ControlMode = CreateMqttValue("ControlMode", "Automatic");

  m_pMqtt_CmdSwitch = CreateMqttCmd("CmdSwitch");

  return true;
}

//! task control
void CLight_1Ch::control(bool bForce /*= false*/) {

  // static unsigned long ulMillis = millis();

  if (m_eLightControlMode != eAutomatic) {
    const int nManualDelay = 2000;
    if (millis() > m_uiTime + nManualDelay) {
      CLed::AddBlinkTask(CLed::BLINK_2);
      m_uiTime += nManualDelay;
    }
    return;
  }

  static long lSecondsFadeStart = 0;
  if (m_uiTime > millis())
    return;

  time_t rawtime;
  struct tm *timeinfo;
  time(&rawtime);
  timeinfo = localtime(&rawtime);
  long lHours = timeinfo->tm_hour;
  long lMinutes = lHours * 60 + timeinfo->tm_min;
  long lSeconds = lMinutes * 60 + timeinfo->tm_sec;
  // long lTimeToStateChange = 0;
  bool bBusy = false;

  switch (m_eControlState) {
  case eInit:
    m_uiTime = millis();
    _log2(CControl::I, "eInit");
    SetLightValue(PIN_WHITE, m_pWhiteValMinP);

    _log(I, "clouds=%s",
         m_pCloudEnabled->m_pTValue->m_Value ? "true" : "false");

    _log(I, "WaitForNtp, Current year is %d", timeinfo->tm_year + 1900);
    m_eControlState = eWaitForNtp;
    m_sState = "Wait for NTP";
    break;

  case eWaitForNtp:
    if (!CControl::ms_bNetworkConnected || timeinfo->tm_year + 1900 < 2010) {
      break;
    }

    m_sState = "Check";
    m_eControlState = eCheck;
    break;

  case eCheck:

    //! find first CState
    m_pCurrentState = m_pEvening_MoonOff;
    while (m_pCurrentState) {
      if (lSeconds > m_pCurrentState->m_pTime->m_lSeconds) {
        m_eControlState = eWaitForFirstTime;
        m_sState = "Wait first time for " + string(m_pCurrentState->m_pcsName);
        _log(I, m_sState.c_str());
        break;
      }
      m_pCurrentState = m_pCurrentState->m_pPrev;
    }
    break;

  case eWaitForFirstTime:
    m_nTimeToStateChangeS = m_pCurrentState->m_pTime->m_lSeconds - lSeconds;
    m_pMqtt_TimeToStateChangeS->setValue(
        TimeToTimeString(m_nTimeToStateChangeS));
    if (m_nTimeToStateChangeS <= 0) {
      m_FadeStateWhite.m_eState = CFadeState::eStart;
      m_sState = "Fade " + string(m_pCurrentState->m_pcsName);
      _log(I, m_sState.c_str());
      m_eControlState = eFade;
    }
    break;

  case eFade:
    m_nTimeToStateChangeS = m_pCurrentState->m_pTime->m_lSeconds +
                            m_pFadeTimeSec->m_lSeconds - lSeconds;
    m_pMqtt_TimeToStateChangeS->setValue(
        TimeToTimeString(m_nTimeToStateChangeS));
    bBusy |= Fade("White", lSeconds, PIN_WHITE, m_pWhiteActive,
                  m_pCurrentState->m_pTargetW, timeinfo,
                  m_FadeStateWhite) == STM_BUSY;
    if (bBusy)
      break;

    _log(I, "%s reached, clouds=%s && %s", m_pCurrentState->m_pcsName,
         m_pCurrentState->m_bClouds ? "true" : "false",
         m_pCloudEnabled->m_pTValue->m_Value ? "true" : "false");
    lSecondsFadeStart = lSeconds;
    if (m_pCurrentState->m_pNext) {
      m_sState = "Wait for " + string(m_pCurrentState->m_pNext->m_pcsName);
      m_eControlState = eWaitForNextTime;
    } else {
      m_sState = "Wait for new day";
      m_eControlState = eWaitForNewDay;
    }
    _log(I, m_sState.c_str());
    break;

  case eWaitForNextTime:
    m_nTimeToStateChangeS =
        m_pCurrentState->m_pNext->m_pTime->m_lSeconds - lSeconds;
    m_pMqtt_TimeToStateChangeS->setValue(
        TimeToTimeString(m_nTimeToStateChangeS));
    if (m_nTimeToStateChangeS <= 0) {
      m_FadeStateWhite.m_eState = CFadeState::eStart;
      m_sState = "Fade " + string(m_pCurrentState->m_pNext->m_pcsName);
      m_pCurrentState = m_pCurrentState->m_pNext;
      _log(I, m_sState.c_str());
      m_eControlState = eFade;
    }
    if (m_pCurrentState->m_bClouds && m_pCloudEnabled->m_pTValue->m_Value) {
      if (lSeconds > (lSecondsFadeStart + m_pCloudeCycle->m_lSeconds)) {
        double d1 = pow(100, m_pCloudChance->m_pTValue->m_Value);
        double dR = random(1, (int)d1);
        double d3 = pow(dR, 1.0 / m_pCloudChance->m_pTValue->m_Value);
        double dValue = dmap(d3, 100, 1, m_pWhiteValMaxP->m_pTValue->m_Value,
                             m_pWhiteValCloudP->m_pTValue->m_Value);
        _log(CControl::I, "analogWrite(%d, %.0f) clouds %.0f %.0f %.0f",
             PIN_WHITE, dValue, d1, dR, d3);
        SetLightValue(PIN_WHITE, dValue);
        lSecondsFadeStart = lSeconds;
      }
    }

    break;

  case eWaitForNewDay:
    m_nTimeToStateChangeS = 24 * 60 * 60 - lSeconds + 10;
    m_pMqtt_TimeToStateChangeS->setValue(
        TimeToTimeString(m_nTimeToStateChangeS));
    if (lSeconds <= 10) {
      _log2(CControl::I, "Change to eCheck");
      m_eControlState = eCheck;
      m_sState = "Check";
    }
    break;
  }
  m_uiTime += 100;

  static uint8 uiModulo = 0;
  if (++uiModulo % 100 == 0)
    CLed::AddBlinkTask(CLed::BLINK_1);
}

_E_STMRESULT CLight_1Ch::Fade(const char *szState, long lSeconds, int nPin,
                              CConfigKeyIntSlider *pValStart,
                              CConfigKeyIntSlider *pValEnd, tm *timeinfo,
                              CFadeState &fadestate) {
  double dValue = 0.0;
  switch (fadestate.m_eState) {
  case CFadeState::eStart:
    if (pValStart == pValEnd) {
      fadestate.m_eState = CFadeState::eDone;
      _log(I, "Abort fade, already target value");
      return STM_DONE;
    }
    {
      char mbstr[100];
      std::strftime(mbstr, sizeof(mbstr), "%A %c", timeinfo);

      _log(CControl::I, "Start Fade %s, takes %d seconds from %d to %d: %s",
           szState, m_pFadeTimeSec->m_lSeconds, pValStart->m_pTValue->m_Value,
           pValEnd->m_pTValue->m_Value, mbstr);
    }
    fadestate.m_lSecondsStart = lSeconds;
    fadestate.m_eState = CFadeState::eProgress;

  case CFadeState::eProgress:

    dValue = dmap(lSeconds, fadestate.m_lSecondsStart,
                  fadestate.m_lSecondsStart + m_pFadeTimeSec->m_lSeconds,
                  pValStart->m_pTValue->m_Value, pValEnd->m_pTValue->m_Value);
    // _log(CControl::I, "analogWrite(%d, %.0f)", nPin, dValue);
    SetLightValue(nPin, dValue);
    if (lSeconds >= fadestate.m_lSecondsStart + m_pFadeTimeSec->m_lSeconds) {
      SetLightValue(nPin, pValEnd);
      _log2(CControl::I, "Fade done");
      fadestate.m_eState = CFadeState::eDone;
    }
    break;
  case CFadeState::eDone:
    return STM_DONE;
  }
  return STM_BUSY;
}

void CLight_1Ch::analogWrite(int nPin, double dValueP) {
  if (dValueP < 0.0)
    dValueP = 0.0;
  else if (dValueP > 100.0)
    dValueP = 100.0;

  ::analogWrite(nPin, (int)(dValueP * 10.23));
  switch (nPin) {
  case PIN_WHITE:
    if (m_dValueWhiteP_ != dValueP) {
      m_pMqtt_LightValueWhite->setValue(std::to_string(dValueP));
      _log(CControl::I, "analogWrite(WHITE, %.2f)", dValueP);
    }
    m_dValueWhiteP_ = dValueP;
    break;

  default:
    break;
  }
}

void CLight_1Ch::SetLightValue(int nPin, CConfigKeyIntSlider *pValue) {
  if (pValue == NULL)
    return;
  double dValueP = pValue->m_pTValue->m_Value;
  analogWrite(nPin, dValueP);
  switch (nPin) {
  case PIN_WHITE:
    m_pWhiteActive = pValue;
    _log(I, "WhiteActive=%s", pValue->GetKey());
    m_dValueWhiteP = dValueP;
    break;

  default:
    break;
  }
}

void CLight_1Ch::SetLightValue(int nPin, double dValueP) {
  analogWrite(nPin, dValueP);

  switch (nPin) {
  case PIN_WHITE:
    m_dValueWhiteP = dValueP;
    break;

  default:
    break;
  }
}

void CLight_1Ch::OnButtonClick() {
  switch (m_eLightControlMode) {
  case eAutomatic:
    SwitchControlMode(eMoon);
    break;

  case eMoon:
    SwitchControlMode(eWhite);
    break;

  case eWhite:
    SwitchControlMode(eOff);
    break;

  case eOff:
    SwitchControlMode(eAutomatic);
    break;

  default:
    break;
  }
}

void CLight_1Ch::SwitchControlMode(E_LIGHTCONTROLMODE eMode) {
  switch (eMode) {
  case eAutomatic:
    m_pMqtt_ControlMode->setValue("Automatic");
    _log2(CControl::I, "ControlMode=Automatic");
    analogWrite(PIN_WHITE, m_dValueWhiteP);
    break;
  case eMoon:
    m_pMqtt_ControlMode->setValue("Moon");
    _log2(CControl::I, "ControlMode=Moon");
    analogWrite(PIN_WHITE, m_pWhiteValMoonP->m_pTValue->m_Value);
    break;
  case eWhite:
    m_pMqtt_ControlMode->setValue("White");
    _log2(CControl::I, "ControlMode=White");
    analogWrite(PIN_WHITE, m_pWhiteValMaxP->m_pTValue->m_Value);
    break;
  case eOff:
    m_pMqtt_ControlMode->setValue("Off");
    _log2(CControl::I, "ControlMode=Off");
    analogWrite(PIN_WHITE, 0);
    break;
  }
  m_eLightControlMode = eMode;
  CLed::AddBlinkTask(CLed::BLINK_1);
}

void CLight_1Ch::ControlMqttCmdCallback(CMqttCmd *pCmd, byte *payload,
                                        unsigned int length) {
  CControl::ControlMqttCmdCallback(pCmd, payload, length);

  if (pCmd == m_pMqtt_CmdSwitch) {
    char szCmd[length + 1];
    strncpy(szCmd, (const char *)payload, length);
    szCmd[length] = 0x00;

    if (strcmp(szCmd, "auto") == 0)
      SwitchControlMode(eAutomatic);
    else if (strcmp(szCmd, "white") == 0)
      SwitchControlMode(eWhite);
    else if (strcmp(szCmd, "moon") == 0)
      SwitchControlMode(eMoon);
    else if (strcmp(szCmd, "off") == 0)
      SwitchControlMode(eOff);
    else
      _log(E, "Unknown CmdSwitch value %s", szCmd);
  }
}

void CLight_1Ch::OnLightValueChanged(CConfigKeyBase *pKey) {
  if (m_eControlState == eInit)
    return;
  _log(I, "OnLightValueChanged %s: %d", pKey->GetKey(),
       static_cast<CConfigKeyIntSlider *>(pKey)->m_pTValue->m_Value);
  if (m_eLightControlMode != eAutomatic) {
    SwitchControlMode(m_eLightControlMode);
  } else {
    if (m_FadeStateWhite.m_eState != CFadeState::eProgress)
      SetLightValue(PIN_WHITE, m_pWhiteActive);
  }
}

void CLight_1Ch::OnEnableChanged(CConfigKeyBase *pKey) {}

void CLight_1Ch::LogCStateChain() {
  unsigned int n = 0;
  CState *pState = m_pMorning_MoonOn;
  while (pState) {
    _log(I, "LogCStateChain: %u: %s", n, pState->m_pcsName);
    pState = pState->m_pNext;
  }
}
