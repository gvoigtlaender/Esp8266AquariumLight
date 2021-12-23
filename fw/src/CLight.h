/* Copyright 2021 Georg Voigtlaender gvoigtlaender@googlemail.com */
#ifndef SRC_CLIGHT_H_
#define SRC_CLIGHT_H_

#include <Arduino.h>
#include <CBase.h>
#include <CConfigValue.h>
#include <CControl.h>
#include <list>

#define CLight_CTRL_VER 2

class CLight : public CControl {
  class CFadeState {
  public:
    enum _E_STATE { eStart = 0, eProgress, eDone };
    CFadeState() : m_eState(eStart), m_lSecondsStart(0) {}
    _E_STATE m_eState;
    long m_lSecondsStart;
  };

public:
  CLight();

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
  CFadeState m_FadeStateBlue;
  void SetLightValue(int nPin, CConfigKeyIntSlider *pValue);
  void SetLightValue(int nPin, double dValueP);
  void analogWrite(int nPin, double dValueP);

  void OnButtonClick();

  E_STATES m_eControlState;
  enum E_LIGHTCONTROLMODE { eAutomatic = 0, eBlue, eWhite, eOff };
  E_LIGHTCONTROLMODE m_eLightControlMode;
  double m_dValueWhiteP;
  double m_dValueBlueP;
  double m_dValueWhiteP_;
  double m_dValueBlueP_;
  int32_t m_nTimeToStateChangeS;

  void SwitchControlMode(E_LIGHTCONTROLMODE eMode);

  CConfigKeyIntSlider *m_pWhiteValMinP = NULL;
  CConfigKeyIntSlider *m_pWhiteValMaxP = NULL;
  CConfigKeyIntSlider *m_pBlueValMinP = NULL;
  CConfigKeyIntSlider *m_pBlueValMaxP = NULL;
  CConfigKeyIntSlider *m_pBlueValWhiteMax = NULL;

  CConfigKeyIntSlider *m_pWhiteActive = NULL;
  CConfigKeyIntSlider *m_pBlueActive = NULL;

  //! DAYLIGHT CONTROL
  CConfigKeyTimeString *m_pFadeTimeSec = NULL;
  CConfigKeyTimeString *m_pTimeBlueOn =
      NULL; // state 1: enable blue in the morning
  CConfigKeyTimeString *m_pTimeWhiteOn =
      NULL; // state 2: enable white in the morning
  CConfigKeyTimeString *m_pTimeWhiteOff =
      NULL; // state 3: disable white in the evening
  CConfigKeyTimeString *m_pTimeBlueOff =
      NULL; // state 4: disable blue in the evening

  //! CLOUD MODE
  CConfigKey<bool> *m_pCloudEnabled = NULL;
  CConfigKeyIntSlider *m_pWhiteValCloudP = NULL;
  CConfigKey<int> *m_pCloudChance = NULL;
  CConfigKeyTimeString *m_pCloudeCycle = NULL;

  //! NOON OFF MODE
  CConfigKey<bool> *m_pNoonOff = NULL;
  CConfigKeyTimeString *m_pTimeNoonOff = NULL;
  CConfigKeyTimeString *m_pTimeNoonOn = NULL;

  CMqttValue *m_pMqtt_LightValueBlue = NULL;
  CMqttValue *m_pMqtt_LightValueWhite = NULL;
  CMqttValue *m_pMqtt_TimeToStateChangeS = NULL;
  CMqttValue *m_pMqtt_ControlMode = NULL;

  CMqttCmd *m_pMqtt_CmdAuto = NULL;
  CMqttCmd *m_pMqtt_CmdWhite = NULL;
  CMqttCmd *m_pMqtt_CmdBlue = NULL;
  CMqttCmd *m_pMqtt_CmdOff = NULL;
  void ControlMqttCmdCallback(CMqttCmd *pCmd, byte *payload,
                              unsigned int length) override;

  string m_sState = "";

  class CState {
  public:
    CState(const char *pcsName, CConfigKeyTimeString *pTime,
           CConfigKeyIntSlider *pTargetW, CConfigKeyIntSlider *pTargetB,
           bool bClouds = false)
        : m_pcsName(pcsName), m_pTime(pTime), m_pTargetW(pTargetW),
          m_pTargetB(pTargetB), m_bClouds(bClouds), m_pPrev(NULL),
          m_pNext(NULL) {}

    const char *m_pcsName;
    CConfigKeyTimeString *m_pTime;

    CConfigKeyIntSlider *m_pTargetW;
    CConfigKeyIntSlider *m_pTargetB;

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

  CState *m_pMorning_BlueOn = NULL;
  CState *m_pMorning_WhiteOn = NULL;
  CState *m_pNoon_AllOff = NULL;
  CState *m_pNoon_AllOn = NULL;
  CState *m_pEvening_WhiteOff = NULL;
  CState *m_pEvening_BlueOff = NULL;

  CState *m_pCurrentState = NULL;

  void OnLightValueChanged(CConfigKeyBase *pKey);
  void OnEnableChanged(CConfigKeyBase *pKey);

  void LogCStateChain();
};

#endif // SRC_CLIGHT_H_