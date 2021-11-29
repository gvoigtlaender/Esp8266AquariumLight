#include <CLed.h>
#include <CLight.h>
#include <CWifi.h>
#include <config.h>
#include <ctime>

#define WITH_NOONMODE 1

void OnLightValueChanged(void *pObject, CConfigKeyBase *pKey) {
  static_cast<CLight *>(pObject)->OnLightValueChanged(pKey);
}
void OnEnableChanged(void *pObject, CConfigKeyBase *pKey) {
  static_cast<CLight *>(pObject)->OnEnableChanged(pKey);
}

CLight::CLight()
    : CControl("CLight"), m_eControlState(eInit),
      m_eLightControlMode(eAutomatic), m_dValueWhiteP(0.0), m_dValueBlueP(0.0),
      m_dValueWhiteP_(0.0), m_dValueBlueP_(0.0) {

  m_pMqtt_LightValueBlue = CreateMqttValue("LightValue.Blue", "0");
  m_pMqtt_LightValueWhite = CreateMqttValue("LightValue.White", "0");
  m_pMqtt_TimeToStateChangeS = CreateMqttValue("TimeToStateChange", "0");
  m_pMqtt_ControlMode = CreateMqttValue("ControlMode", "Automatic");
  pinMode(PIN_BLUE, OUTPUT);
  pinMode(PIN_WHITE, OUTPUT);
  digitalWrite(PIN_BLUE, LOW);
  digitalWrite(PIN_WHITE, LOW);

  analogWriteFreq(25000);
  analogWriteRange(PWM_RANGE);

  m_pWhiteValMinP =
      CreateConfigKeyIntSlider("Light", "WhiteValueMin", 0, 0, 100);
  m_pWhiteValMinP->SetOnChangedCallback(::OnLightValueChanged, this);

  m_pWhiteValMaxP =
      CreateConfigKeyIntSlider("Light", "WhiteValueMax", 100, 0, 100);
  m_pWhiteValMaxP->SetOnChangedCallback(::OnLightValueChanged, this);

  m_pBlueValMinP = CreateConfigKeyIntSlider("Light", "BlueValueMin", 0, 0, 100);
  m_pBlueValMinP->SetOnChangedCallback(::OnLightValueChanged, this);

  m_pBlueValMaxP =
      CreateConfigKeyIntSlider("Light", "BlueValueMax", 100, 0, 100);
  m_pBlueValMaxP->SetOnChangedCallback(::OnLightValueChanged, this);

  m_pBlueValWhiteMax =
      CreateConfigKeyIntSlider("Light", "BlueValWhiteMax", 100, 0, 100);
  m_pBlueValWhiteMax->SetOnChangedCallback(::OnLightValueChanged, this);

  //! DAYLIGHT CONTROL
  m_pFadeTimeSec = CreateConfigKeyTimeString("Timing", "FadeTime", "5:00");
  m_pTimeBlueOn = CreateConfigKeyTimeString("Timing", "BlueOn", "07:00");
  m_pTimeWhiteOn = CreateConfigKeyTimeString("Timing", "WhiteOn", "08:00");
  m_pTimeWhiteOff = CreateConfigKeyTimeString("Timing", "WhiteOff", "18:00");
  m_pTimeBlueOff = CreateConfigKeyTimeString("Timing", "BlueOff", "22:00");

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

  m_pMorning_BlueOn = new CState("Morning_BlueOn", m_pTimeBlueOn,
                                 m_pWhiteValMinP, m_pBlueValMaxP);
  m_pMorning_WhiteOn = new CState("Morning_WhiteOn", m_pTimeWhiteOn,
                                  m_pWhiteValMaxP, m_pBlueValWhiteMax, true);
  m_pNoon_AllOff = new CState("Noon_AllOff", m_pTimeNoonOff, m_pWhiteValMinP,
                              m_pBlueValMinP);
  m_pNoon_AllOn = new CState("Noon_AllOn", m_pTimeNoonOn, m_pWhiteValMaxP,
                             m_pBlueValWhiteMax, true);
  m_pEvening_WhiteOff = new CState("Evening_WhiteOff", m_pTimeWhiteOff,
                                   m_pWhiteValMinP, m_pBlueValMaxP);
  m_pEvening_BlueOff = new CState("Evening_BlueOff", m_pTimeBlueOff,
                                  m_pWhiteValMinP, m_pBlueValMinP);

  m_pMorning_BlueOn->SetNext(m_pMorning_WhiteOn);
  m_pMorning_WhiteOn->SetNext(m_pNoon_AllOff);
  m_pNoon_AllOff->SetNext(m_pNoon_AllOn);
  m_pNoon_AllOn->SetNext(m_pEvening_WhiteOff);
  m_pEvening_WhiteOff->SetNext(m_pEvening_BlueOff);

  LogCStateChain();
}

bool CLight::setup() { return true; }

//! task control
#if CLight_CTRL_VER == 1
void CLight::control(bool bForce /*= false*/) {

  static unsigned long ulMillis = millis();

  if (m_eLightControlMode != eAutomatic) {
    const int nManualDelay = 2000;
    if (millis() > ulMillis + nManualDelay) {
      CLed::AddBlinkTask(CLed::BLINK_2);
      ulMillis += nManualDelay;
    }
    return;
  }

  static long lSecondsFadeStart = 0;
  if (ulMillis > millis())
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
    ulMillis = millis();
    _log2(CControl::I, "eInit");
    SetLightValue(PIN_BLUE, m_pBlueValMinP);
    SetLightValue(PIN_WHITE, m_pWhiteValMinP);

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

    if (lSeconds > m_pTimeBlueOff->m_lSeconds) {
      _log(CControl::I, "Change to eSEnd %ld > %ld", lSeconds,
           m_pTimeBlueOff->m_lSeconds);
      SetLightValue(PIN_BLUE, m_pBlueValMinP);
      SetLightValue(PIN_WHITE, m_pWhiteValMinP);
      m_eControlState = eWaitForNewDay;
      m_sState = "Wait for new day";
    } else if (lSeconds > m_pTimeWhiteOff->m_lSeconds) {
      _log(CControl::I, "Change to eWaitForBlueOff %ld > %ld", lSeconds,
           m_pTimeWhiteOff->m_lSeconds);
      SetLightValue(PIN_BLUE, m_pBlueValMaxP);
      SetLightValue(PIN_WHITE, m_pWhiteValMinP);
      m_eControlState = eWaitForBlueOff;
      m_sState = "Wait for BlueOff";
    } else if (lSeconds > m_pTimeWhiteOn->m_lSeconds) {
      _log(CControl::I, "Change to eWhiteOnCheck %ld > %ld", lSeconds,
           m_pTimeWhiteOn->m_lSeconds);
      SetLightValue(PIN_BLUE, m_pBlueValWhiteMax);
      SetLightValue(PIN_WHITE, m_pWhiteValMaxP);
      m_eControlState = eWhiteOnCheck;
      m_sState = "Wait for WhiteOnCheck";
    } else if (lSeconds > m_pTimeBlueOn->m_lSeconds) {
      _log(CControl::I, "Change to eWaitForWhiteOn %ld > %ld", lSeconds,
           m_pTimeBlueOn->m_lSeconds);
      SetLightValue(PIN_BLUE, m_pBlueValMaxP);
      SetLightValue(PIN_WHITE, m_pWhiteValMinP);
      m_eControlState = eWaitForWhiteOn;
      m_sState = "Wait for WhiteOn";
    } else {
      _log(CControl::I, "Change to eWaitForBlueOn %ld", lSeconds);
      SetLightValue(PIN_BLUE, m_pBlueValMinP);
      SetLightValue(PIN_WHITE, m_pWhiteValMinP);
      m_eControlState = eWaitForBlueOn;
      m_sState = "Wait for BlueOn";
    }
    break;

  case eWaitForBlueOn: // state 1: enable blue in the morning
    m_nTimeToStateChangeS = m_pTimeBlueOn->m_lSeconds - lSeconds;
    m_pMqtt_TimeToStateChangeS->setValue(std::to_string(m_nTimeToStateChangeS));
    if (m_nTimeToStateChangeS <= 0) {
      m_eControlState = eBlueOn;
      m_sState = "Fade BlueOn";
      m_FadeStateBlue.m_eState = CFadeState::eStart;
    }
    break;

  case eBlueOn:
    if (Fade("BlueOn", lSeconds, PIN_BLUE, m_pBlueValMinP, m_pBlueValMaxP,
             timeinfo, m_FadeStateBlue) == STM_BUSY)
      break;
    lSecondsFadeStart = lSeconds;
    m_eControlState = eWaitForWhiteOn;
    m_sState = "Wait for WhiteOn";
    break;

  case eWaitForWhiteOn: // state 1: enable blue in the morning
    m_nTimeToStateChangeS = m_pTimeWhiteOn->m_lSeconds - lSeconds;
    m_pMqtt_TimeToStateChangeS->setValue(std::to_string(m_nTimeToStateChangeS));
    if (m_nTimeToStateChangeS <= 0) {
      m_eControlState = eWhiteOn;
      m_sState = "Fade WhiteOn";
      m_FadeStateWhite.m_eState = CFadeState::eStart;
      m_FadeStateBlue.m_eState = CFadeState::eStart;
    }
    break;

  case eWhiteOn:
    m_nTimeToStateChangeS =
        m_pTimeWhiteOn->m_lSeconds + m_pFadeTimeSec->m_lSeconds - lSeconds;
    m_pMqtt_TimeToStateChangeS->setValue(std::to_string(m_nTimeToStateChangeS));
    bBusy |= Fade("WhiteOn", lSeconds, PIN_WHITE, m_pWhiteValMinP,
                  m_pWhiteValMaxP, timeinfo, m_FadeStateWhite) == STM_BUSY;
    bBusy |= Fade("BlueWhiteOn", lSeconds, PIN_BLUE, m_pBlueValMaxP,
                  m_pBlueValWhiteMax, timeinfo, m_FadeStateBlue) == STM_BUSY;

    if (bBusy)
      break;
    lSecondsFadeStart = lSeconds;
    m_eControlState = eWhiteOnCheck;
    break;

  case eWhiteOnCheck:

#if WITH_NOONMODE == 1
    if (m_pTimeNoonOn->m_lSeconds < lSeconds) {
      SetLightValue(PIN_BLUE, m_pBlueValMinP);
      SetLightValue(PIN_WHITE, m_pWhiteValMinP);
      m_eControlState = eWaitForNoonOn;
      m_sState = "Wait for NoonOn";
    } else if (m_pTimeNoonOff->m_lSeconds < lSeconds) {
      m_eControlState = eWaitForNoonOff;
      m_sState = "Wait for NoonOff";
    } else {

#else
  {
#endif
      m_eControlState = eWaitForWhiteOff;
      m_sState = "Wait for WhiteOff";
    }
    _log(I, m_sState.c_str());
    break;

  case eWaitForNoonOff:
    m_nTimeToStateChangeS = m_pTimeNoonOff->m_lSeconds - lSeconds;
    m_pMqtt_TimeToStateChangeS->setValue(std::to_string(m_nTimeToStateChangeS));
    if (m_nTimeToStateChangeS <= 0) {
      m_eControlState = eNoonOff;
      m_sState = "Fade NoonOff";
      m_FadeStateWhite.m_eState = CFadeState::eStart;
      m_FadeStateBlue.m_eState = CFadeState::eStart;
      break;
    }
    if (m_pCloudEnabled->m_pTValue->m_Value) {
      if (lSeconds > (lSecondsFadeStart + m_pCloudeCycle->m_lSeconds)) {
        double d1 = pow(100, m_pCloudChance->m_pTValue->m_Value);
        double dR = random(1, (int)d1);
        double d3 = pow(dR, 1.0 / m_pCloudChance->m_pTValue->m_Value);
        double dValue = dmap(d3, 100, 1, m_pWhiteValMaxP->m_pTValue->m_Value,
                             m_pWhiteValCloudP->m_pTValue->m_Value);
        _log(CControl::I, "analogWrite(%d, %.0f) clouds %.0f %.0f %.0f",
             PIN_WHITE, dValue, d1, dR, d3);
        SetLightValue(PIN_WHITE, (int)dValue);
        lSecondsFadeStart = lSeconds;
      }
    }
    break;

  case eNoonOff:
    m_nTimeToStateChangeS =
        m_pTimeNoonOff->m_lSeconds + m_pFadeTimeSec->m_lSeconds - lSeconds;
    m_pMqtt_TimeToStateChangeS->setValue(std::to_string(m_nTimeToStateChangeS));
    bBusy |= Fade("WhiteOff", lSeconds, PIN_WHITE, m_pWhiteValMaxP,
                  m_pWhiteValMinP, timeinfo, m_FadeStateWhite) == STM_BUSY;
    bBusy |= Fade("BlueOff", lSeconds, PIN_BLUE, m_pBlueValWhiteMax,
                  m_pBlueValMinP, timeinfo, m_FadeStateBlue) == STM_BUSY;

    if (bBusy)
      break;
    lSecondsFadeStart = lSeconds;
    m_eControlState = eWaitForNoonOn;
    m_sState = "Wait for NoonOn";
    break;

  case eWaitForNoonOn:
    m_nTimeToStateChangeS = m_pTimeNoonOn->m_lSeconds - lSeconds;
    m_pMqtt_TimeToStateChangeS->setValue(std::to_string(m_nTimeToStateChangeS));
    if (m_nTimeToStateChangeS <= 0) {
      m_eControlState = eNoonOn;
      m_sState = "Fade NoonOn";
      m_FadeStateWhite.m_eState = CFadeState::eStart;
      m_FadeStateBlue.m_eState = CFadeState::eStart;
      break;
    }

  case eNoonOn:
    m_nTimeToStateChangeS =
        m_pTimeNoonOn->m_lSeconds + m_pFadeTimeSec->m_lSeconds - lSeconds;
    m_pMqtt_TimeToStateChangeS->setValue(std::to_string(m_nTimeToStateChangeS));
    bBusy |= Fade("WhiteOn", lSeconds, PIN_WHITE, m_pWhiteValMinP,
                  m_pWhiteValMaxP, timeinfo, m_FadeStateWhite) == STM_BUSY;
    bBusy |= Fade("BlueWhiteOn", lSeconds, PIN_BLUE, m_pBlueValMinP,
                  m_pBlueValWhiteMax, timeinfo, m_FadeStateBlue) == STM_BUSY;

    if (bBusy)
      break;
    lSecondsFadeStart = lSeconds;
    m_eControlState = eWaitForWhiteOff;
    m_sState = "Wait for WhiteOff";
    break;

  case eWaitForWhiteOff: // state 1: enable blue in the morning
    m_nTimeToStateChangeS = m_pTimeWhiteOff->m_lSeconds - lSeconds;
    m_pMqtt_TimeToStateChangeS->setValue(std::to_string(m_nTimeToStateChangeS));
    if (m_nTimeToStateChangeS <= 0) {
      m_eControlState = eWhiteOff;
      m_sState = "Fade WhiteOff";
      m_FadeStateWhite.m_eState = CFadeState::eStart;
      m_FadeStateBlue.m_eState = CFadeState::eStart;
      break;
    }
    if (m_pCloudEnabled->m_pTValue->m_Value) {
      if (lSeconds > (lSecondsFadeStart + m_pCloudeCycle->m_lSeconds)) {
        double d1 = pow(100, m_pCloudChance->m_pTValue->m_Value);
        double dR = random(1, (int)d1);
        double d3 = pow(dR, 1.0 / m_pCloudChance->m_pTValue->m_Value);
        double dValue = dmap(d3, 100, 1, m_pWhiteValMaxP->m_pTValue->m_Value,
                             m_pWhiteValCloudP->m_pTValue->m_Value);
        _log(CControl::I, "analogWrite(%d, %.0f) clouds %.0f %.0f %.0f",
             PIN_WHITE, dValue, d1, dR, d3);
        SetLightValue(PIN_WHITE, (int)dValue);
        lSecondsFadeStart = lSeconds;
      }
    }
    break;

  case eWhiteOff:
    m_nTimeToStateChangeS =
        m_pTimeWhiteOff->m_lSeconds + m_pFadeTimeSec->m_lSeconds - lSeconds;
    m_pMqtt_TimeToStateChangeS->setValue(std::to_string(m_nTimeToStateChangeS));
    bBusy |= Fade("WhiteOff", lSeconds, PIN_WHITE, m_pWhiteValMaxP,
                  m_pWhiteValMinP, timeinfo, m_FadeStateWhite) == STM_BUSY;
    bBusy |= Fade("BlueWhiteOff", lSeconds, PIN_BLUE, m_pBlueValWhiteMax,
                  m_pBlueValMaxP, timeinfo, m_FadeStateBlue) == STM_BUSY;

    if (bBusy)
      break;
    lSecondsFadeStart = lSeconds;
    m_eControlState = eWaitForBlueOff;
    m_sState = "Wait for BlueOff";
    break;

  case eWaitForBlueOff: // state 1: enable blue in the morning
    m_nTimeToStateChangeS = m_pTimeBlueOff->m_lSeconds - lSeconds;
    m_pMqtt_TimeToStateChangeS->setValue(std::to_string(m_nTimeToStateChangeS));
    if (m_nTimeToStateChangeS <= 0) {
      m_eControlState = eBlueOff;
      m_sState = "Fade BlueOff";
      m_FadeStateBlue.m_eState = CFadeState::eStart;
    }
    break;

  case eBlueOff:
    m_nTimeToStateChangeS =
        m_pTimeBlueOff->m_lSeconds + m_pFadeTimeSec->m_lSeconds - lSeconds;
    m_pMqtt_TimeToStateChangeS->setValue(std::to_string(m_nTimeToStateChangeS));
    if (Fade("BlueOff", lSeconds, PIN_BLUE, m_pBlueValMaxP, m_pBlueValMinP,
             timeinfo, m_FadeStateBlue) == STM_BUSY)
      break;
    m_pMqtt_TimeToStateChangeS->setValue(std::to_string(0));
    lSecondsFadeStart = lSeconds;
    m_eControlState = eWaitForNewDay;
    m_sState = "Wait for new day";
    break;

  case eWaitForNewDay:
    m_nTimeToStateChangeS = 24 * 60 * 60 - lSeconds + 10;
    m_pMqtt_TimeToStateChangeS->setValue(std::to_string(m_nTimeToStateChangeS));
    if (lSeconds <= 10) {
      _log2(CControl::I, "Change to eCheck");
      m_eControlState = eCheck;
      m_sState = "Check";
    }
    break;
  }
  ulMillis += 100;

  static uint8 uiModulo = 0;
  if (++uiModulo % 100 == 0)
    CLed::AddBlinkTask(CLed::BLINK_1);
}
#else
void CLight::control(bool bForce /*= false*/) {

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
    SetLightValue(PIN_BLUE, m_pBlueValMinP);
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
    m_pCurrentState = m_pEvening_BlueOff;
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
    m_pMqtt_TimeToStateChangeS->setValue(std::to_string(m_nTimeToStateChangeS));
    if (m_nTimeToStateChangeS <= 0) {
      m_FadeStateWhite.m_eState = CFadeState::eStart;
      m_FadeStateBlue.m_eState = CFadeState::eStart;
      m_sState = "Fade " + string(m_pCurrentState->m_pcsName);
      _log(I, m_sState.c_str());
      m_eControlState = eFade;
    }
    break;

  case eFade:
    m_nTimeToStateChangeS = m_pCurrentState->m_pTime->m_lSeconds +
                            m_pFadeTimeSec->m_lSeconds - lSeconds;
    m_pMqtt_TimeToStateChangeS->setValue(std::to_string(m_nTimeToStateChangeS));
    bBusy |= Fade("White", lSeconds, PIN_WHITE, m_pWhiteActive,
                  m_pCurrentState->m_pTargetW, timeinfo,
                  m_FadeStateWhite) == STM_BUSY;
    bBusy |= Fade("Blue", lSeconds, PIN_BLUE, m_pBlueActive,
                  m_pCurrentState->m_pTargetB, timeinfo,
                  m_FadeStateBlue) == STM_BUSY;

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
    m_pMqtt_TimeToStateChangeS->setValue(std::to_string(m_nTimeToStateChangeS));
    if (m_nTimeToStateChangeS <= 0) {
      m_FadeStateWhite.m_eState = CFadeState::eStart;
      m_FadeStateBlue.m_eState = CFadeState::eStart;
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
        SetLightValue(PIN_WHITE, (int)dValue);
        lSecondsFadeStart = lSeconds;
      }
    }

    break;

  case eWaitForNewDay:
    m_nTimeToStateChangeS = 24 * 60 * 60 - lSeconds + 10;
    m_pMqtt_TimeToStateChangeS->setValue(std::to_string(m_nTimeToStateChangeS));
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
#endif

_E_STMRESULT CLight::Fade(const char *szState, long lSeconds, int nPin,
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
    SetLightValue(nPin, (int)dValue);
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

void CLight::analogWrite(int nPin, double dValueP) {
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

  case PIN_BLUE:
    if (m_dValueBlueP_ != dValueP) {
      m_pMqtt_LightValueBlue->setValue(std::to_string(dValueP));
      _log(CControl::I, "analogWrite(BLUE, %.2f)", dValueP);
    }
    m_dValueBlueP_ = dValueP;
    break;

  default:
    break;
  }
}

void CLight::SetLightValue(int nPin, CConfigKeyIntSlider *pValue) {
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

  case PIN_BLUE:
    m_pBlueActive = pValue;
    _log(I, "BlueActive=%s", pValue->GetKey());
    m_dValueBlueP = dValueP;
    break;

  default:
    break;
  }
}

void CLight::SetLightValue(int nPin, double dValueP) {
  analogWrite(nPin, dValueP);

  switch (nPin) {
  case PIN_WHITE:
    m_dValueWhiteP = dValueP;
    break;

  case PIN_BLUE:
    m_dValueBlueP = dValueP;
    break;

  default:
    break;
  }
}

void CLight::OnButtonClick() {
  switch (m_eLightControlMode) {
  case eAutomatic:
    SwitchControlMode(eBlue);
    break;

  case eBlue:
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

void CLight::SwitchControlMode(E_LIGHTCONTROLMODE eMode) {
  switch (eMode) {
  case eAutomatic:
    m_pMqtt_ControlMode->setValue("Automatic");
    _log2(CControl::I, "ControlMode=Automatic");
    analogWrite(PIN_BLUE, m_dValueBlueP);
    analogWrite(PIN_WHITE, m_dValueWhiteP);
    break;
  case eBlue:
    m_pMqtt_ControlMode->setValue("Blue");
    _log2(CControl::I, "ControlMode=Blue");
    analogWrite(PIN_BLUE, m_pBlueValMaxP->m_pTValue->m_Value);
    analogWrite(PIN_WHITE, 0);
    break;
  case eWhite:
    m_pMqtt_ControlMode->setValue("White");
    _log2(CControl::I, "ControlMode=White");
    analogWrite(PIN_BLUE, 0);
    analogWrite(PIN_WHITE, m_pWhiteValMaxP->m_pTValue->m_Value);
    break;
  case eOff:
    m_pMqtt_ControlMode->setValue("Off");
    _log2(CControl::I, "ControlMode=Off");
    analogWrite(PIN_BLUE, 0);
    analogWrite(PIN_WHITE, 0);
    break;
  }
  m_eLightControlMode = eMode;
  CLed::AddBlinkTask(CLed::BLINK_1);
}

void CLight::OnLightValueChanged(CConfigKeyBase *pKey) {
  if (m_eControlState == eInit)
    return;
  _log(I, "OnLightValueChanged %s: %d", pKey->GetKey(),
       static_cast<CConfigKeyIntSlider *>(pKey)->m_pTValue->m_Value);
  if (m_eLightControlMode != eAutomatic) {
    SwitchControlMode(m_eLightControlMode);
  } else {
    if (m_FadeStateWhite.m_eState != CFadeState::eProgress)
      SetLightValue(PIN_WHITE, m_pWhiteActive);
    if (m_FadeStateBlue.m_eState != CFadeState::eProgress)
      SetLightValue(PIN_BLUE, m_pBlueActive);
  }
}

void CLight::OnEnableChanged(CConfigKeyBase *pKey) {
  if (pKey == m_pNoonOff) {
    if (m_pNoonOff->GetValue()) {
      _log(I, "CLight::OnEnableChanged: NoonMode = true");
      m_pMorning_WhiteOn->m_pNext = m_pNoon_AllOff;
      m_pEvening_WhiteOff->m_pPrev = m_pNoon_AllOn;
    } else {
      _log(I, "CLight::OnEnableChanged: NoonMode = false");
      m_pMorning_WhiteOn->m_pNext = m_pEvening_WhiteOff;
      m_pEvening_WhiteOff->m_pPrev = m_pMorning_WhiteOn;
    }
    if (m_eControlState == eWaitForNextTime)
      m_sState = "Wait for " + string(m_pCurrentState->m_pNext->m_pcsName);
    LogCStateChain();
  }
}

void CLight::LogCStateChain() {
  unsigned int n = 0;
  CState *pState = m_pMorning_BlueOn;
  while (pState) {
    _log(I, "LogCStateChain: %u: %s", n, pState->m_pcsName);
    pState = pState->m_pNext;
  }
}
