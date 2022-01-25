/* Copyright 2021 Georg Voigtlaender gvoigtlaender@googlemail.com */
#ifndef SRC_CLIGHT_H_
#define SRC_CLIGHT_H_

#include <Arduino.h>
#include <CBase.h>
#include <CConfigValue.h>
#include <CControl.h>
#include <list>

#define CLight_CTRL_VER 2

class CLight_1Ch : public CControl {
  class CFadeState {
  public:
    enum _E_STATE { eStart = 0, eProgress, eDone };
    CFadeState() : m_eState(eStart), m_lSecondsStart(0) {}
    _E_STATE m_eState;
    long m_lSecondsStart;
  };

public:
  CLight_1Ch();

  bool setup() override;

//! task control
#if CLight_CTRL_VER == 1
  void control(bool bForce /*= false*/) override;
  enum E_STATES{eInit = 0,        eWaitForNtp,    eCheck,
                eWaitForBlueOn,   eBlueOn,        eWaitForWhiteOn,
                eWhiteOn,         eWhiteOnCheck,  eWaitForNoonOff,
                eNoonOff,         eWaitForNoonOn, eNoonOn,
                eWaitForWhiteOff, eWhiteOff,      eWaitForBlueOff,
                eBlueOff,         eWaitForNewDay};
#else
  void control(bool bForce /*= false*/) override;
  enum E_STATES {
    eInit = 0,
    eWaitForNtp,
    eCheck,
    eWaitForFirstTime,
    eFade,
    eWaitForNextTime,
    eWaitForNewDay
  };
#endif

  _E_STMRESULT Fade(const char *szState, long lSeconds, int nPin,
                    CConfigKeyIntSlider *pValStart,
                    CConfigKeyIntSlider *pValEnd, tm *timeinfo,
                    CFadeState &fadestate);
  CFadeState m_FadeStateWhite;
  void SetLightValue(int nPin, CConfigKeyIntSlider *pValue);
  void SetLightValue(int nPin, double dValueP);
  void analogWrite(int nPin, double dValueP);

  void OnButtonClick();

  E_STATES m_eControlState;
  enum E_LIGHTCONTROLMODE { eAutomatic = 0, eMoon, eWhite, eOff };
  E_LIGHTCONTROLMODE m_eLightControlMode;
  double m_dValueWhiteP;
  double m_dValueWhiteP_;
  int32_t m_nTimeToStateChangeS;

  void SwitchControlMode(E_LIGHTCONTROLMODE eMode);

  CConfigKeyIntSlider *m_pWhiteValMinP = NULL;
  CConfigKeyIntSlider *m_pWhiteValMaxP = NULL;
  CConfigKeyIntSlider *m_pWhiteValMoonP = NULL;

  CConfigKeyIntSlider *m_pWhiteActive = NULL;

  //! DAYLIGHT CONTROL
  CConfigKeyTimeString *m_pFadeTimeSec = NULL;
  CConfigKeyTimeString *m_pTimeMoonOn =
      NULL; // state 1: enable moon in the morning
  CConfigKeyTimeString *m_pTimeWhiteOn =
      NULL; // state 2: enable white in the morning
  CConfigKeyTimeString *m_pTimeWhiteOff =
      NULL; // state 3: disable white in the evening
  CConfigKeyTimeString *m_pTimeMoonOff =
      NULL; // state 4: disable moon in the evening

  //! CLOUD MODE
  CConfigKey<bool> *m_pCloudEnabled = NULL;
  CConfigKeyIntSlider *m_pWhiteValCloudP = NULL;
  CConfigKey<int> *m_pCloudChance = NULL;
  CConfigKeyTimeString *m_pCloudeCycle = NULL;

  CMqttValue *m_pMqtt_LightValueWhite = NULL;
  CMqttValue *m_pMqtt_TimeToStateChangeS = NULL;
  CMqttValue *m_pMqtt_ControlMode = NULL;

  CMqttCmd *m_pMqtt_CmdSwitch = NULL;
  void ControlMqttCmdCallback(CMqttCmd *pCmd, byte *payload,
                              unsigned int length) override;

  string m_sState = "";

  class CState {
  public:
    CState(const char *pcsName, CConfigKeyTimeString *pTime,
           CConfigKeyIntSlider *pTargetW, bool bClouds = false)
        : m_pcsName(pcsName), m_pTime(pTime), m_pTargetW(pTargetW),
          m_bClouds(bClouds), m_pPrev(NULL), m_pNext(NULL) {}

    const char *m_pcsName;
    CConfigKeyTimeString *m_pTime;

    CConfigKeyIntSlider *m_pTargetW;

    bool m_bClouds;

    CState *m_pPrev;
    CState *m_pNext;

    void SetPrev(CState *pPrev) {
      m_pPrev = pPrev;
      if (pPrev)
        pPrev->m_pNext = this;
    }
    void SetNext(CState *pNext) {
      m_pNext = pNext;
      if (pNext)
        pNext->m_pPrev = this;
    }
  };

  CState *m_pMorning_MoonOn = NULL;
  CState *m_pMorning_WhiteOn = NULL;
  CState *m_pEvening_WhiteOff = NULL;
  CState *m_pEvening_MoonOff = NULL;

  CState *m_pCurrentState = NULL;

  void OnLightValueChanged(CConfigKeyBase *pKey);
  void OnEnableChanged(CConfigKeyBase *pKey);

  void LogCStateChain();
};

#endif // SRC_CLIGHT_H_