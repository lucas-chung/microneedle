#include <Arduino.h>
#include <SPI.h>
#include <math.h>
#include <esp_system.h>

extern "C" {
#include "ad5940.h"
#include "Amperometric.h"
#include "ad5940_port.h"
}

#include "board_pins.h"

static const uint32_t SERIAL_BAUD = 115200;

static const uint8_t RAW_SPICMD_SETADDR = 0x20;
static const uint8_t RAW_SPICMD_READREG = 0x6D;
static const uint16_t RAW_REG_ADIID = 0x0400;
static const uint16_t RAW_REG_CHIPID = 0x0404;
static SPISettings g_rawAd5941Spi(1000000, MSBFIRST, SPI_MODE0);

static const uint32_t SEQ_BUFFER_WORDS = 512;
static uint32_t seqBuffer[SEQ_BUFFER_WORDS];
static const uint8_t MAX_WE_CHANNELS = 3;
static const uint16_t DEFAULT_SEGMENTS = 1;

enum RtiaSource : uint8_t {
  RTIA_SOURCE_INTERNAL = 0,
  RTIA_SOURCE_EXT_AIN3_2K = 1,
  RTIA_SOURCE_EXT_AIN2_25K5 = 2,
  RTIA_SOURCE_EXT_AIN1_470K = 3
};

struct DpvParams {
  int weChannel = 1;
  float startMv = -200.0f;
  float endMv = 600.0f;
  float stepMv = 5.0f;
  float pulseAmpMv = 50.0f;
  float scanRateMvS = 50.0f;
  uint32_t pulseMs = 50;
  uint32_t quietMs = 2000;
  uint32_t stepSettleMs = 20;
  float vzeroMv = 1100.0f;
  float rcalOhm = 200.0f;
  float rtiaOhm = 4000.0f;
  RtiaSource rtiaSource = RTIA_SOURCE_INTERNAL;
  float adcRefVolt = 1.8162f;
  float adcPga = 1.5f;
  uint32_t sampleTimeoutMs = 500;
  uint16_t maxPoints = 500;
};

struct DpvPoint {
  uint32_t tMs;
  uint16_t segment;
  uint16_t index;
  int weChannel;
  float eBaseMv;
  float ePulseMv;
  float iBaseUa;
  float iPulseUa;
  float dIUa;
};

struct DpvRunState {
  bool active;
  bool done;
  uint16_t index;
  uint16_t points;
  float eBaseMv;
  float stepMv;
  uint32_t stepPeriodMs;
  uint32_t nextDueMs;
};

static DpvParams params;
static DpvParams weParams[MAX_WE_CHANNELS + 1];
static bool selectedWe[MAX_WE_CHANNELS + 1] = {false, true, false, false};
static uint16_t weSegments[MAX_WE_CHANNELS + 1] = {0, DEFAULT_SEGMENTS, DEFAULT_SEGMENTS, DEFAULT_SEGMENTS};
static bool portReady = false;
static bool afeReady = false;
static bool afeInitTried = false;
static bool ad5940Initialized = false;
static bool runRequested = false;
static bool running = false;
static uint16_t pointCount = 0;
static int32_t lastAfeError = 0;
static uint32_t lastRawAdiid = 0;
static uint32_t lastRawChipid = 0;
static uint16_t lastAdiid = 0;
static uint16_t lastChipid = 0;
static String serialLine;
static String lastErrorCode = "OK";
static String lastErrorMessage = "";
static String paramErrorCode = "";
static String paramErrorMessage = "";

static const char *resetReasonText(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_EXT: return "EXTERNAL_RESET";
    case ESP_RST_SW: return "SOFTWARE_RESET";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WDT";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT: return "OTHER_WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_SDIO: return "SDIO";
    default: return "UNKNOWN";
  }
}

static float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static String finiteText(float v, uint8_t digits) {
  if (!isfinite(v)) return "nan";
  return String(v, (unsigned int)digits);
}

static void printEvent(const char *name) {
  Serial.print("#EVENT,");
  Serial.println(name);
}

static void printEventValue(const char *name, const String &value) {
  Serial.print("#EVENT,");
  Serial.print(name);
  Serial.print(",");
  Serial.println(value);
}

static void rememberError(const String &code, const String &message) {
  lastErrorCode = code;
  lastErrorMessage = message;
}

static void printErrorDetail(const String &code, const String &message) {
  rememberError(code, message);
  Serial.print("#ERROR,");
  Serial.print(code);
  if (message.length() > 0) {
    Serial.print(",");
    Serial.print(message);
  }
  Serial.println();
}

static void printError(const char *code) {
  printErrorDetail(code, "");
}

static void setParamError(const String &code, const String &message) {
  paramErrorCode = code;
  paramErrorMessage = message;
}

static void clearParamError() {
  paramErrorCode = "";
  paramErrorMessage = "";
}

static bool parseStrictFloat(const String &raw, float *out) {
  String s = raw;
  s.trim();
  if (s.length() == 0) return false;
  bool seenDigit = false;
  bool seenDot = false;
  for (uint16_t i = 0; i < s.length(); i++) {
    char c = s.charAt(i);
    if (c >= '0' && c <= '9') {
      seenDigit = true;
      continue;
    }
    if ((c == '+' || c == '-') && i == 0) continue;
    if (c == '.' && !seenDot) {
      seenDot = true;
      continue;
    }
    return false;
  }
  if (!seenDigit) return false;
  *out = s.toFloat();
  return isfinite(*out);
}

static bool rangeOk(const String &key, float value, float lo, float hi) {
  if (value >= lo && value <= hi) return true;
  setParamError("PARAM_RANGE", key + "_must_be_" + String(lo, 3) + "_to_" + String(hi, 3));
  return false;
}

static void keepTfDeselected() {
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);
}

static uint32_t rawAd5941ReadReg32(uint16_t regAddr) {
  keepTfDeselected();
  digitalWrite(PIN_AD5941_CS, HIGH);
  SPI.begin(PIN_AD5941_SCLK, PIN_AD5941_MISO, PIN_AD5941_MOSI, PIN_AD5941_CS);
  SPI.beginTransaction(g_rawAd5941Spi);

  digitalWrite(PIN_AD5941_CS, LOW);
  SPI.transfer(RAW_SPICMD_SETADDR);
  SPI.transfer((regAddr >> 8) & 0xFF);
  SPI.transfer(regAddr & 0xFF);
  digitalWrite(PIN_AD5941_CS, HIGH);

  delayMicroseconds(10);

  digitalWrite(PIN_AD5941_CS, LOW);
  SPI.transfer(RAW_SPICMD_READREG);
  uint32_t v = 0;
  v |= ((uint32_t)SPI.transfer(0xFF)) << 24;
  v |= ((uint32_t)SPI.transfer(0xFF)) << 16;
  v |= ((uint32_t)SPI.transfer(0xFF)) << 8;
  v |= ((uint32_t)SPI.transfer(0xFF));
  digitalWrite(PIN_AD5941_CS, HIGH);

  SPI.endTransaction();
  keepTfDeselected();
  return v;
}

static uint16_t rawAd5941Extract16(uint32_t raw) {
  uint16_t mid = (uint16_t)((raw >> 8) & 0xFFFF);
  uint16_t low = (uint16_t)(raw & 0xFFFF);
  if (mid == AD5940_ADIID || mid == 0x5502 || mid == 0x5501 || mid == 0x5500) return mid;
  return low;
}

static bool readAd5941Identity() {
  lastRawAdiid = rawAd5941ReadReg32(RAW_REG_ADIID);
  lastRawChipid = rawAd5941ReadReg32(RAW_REG_CHIPID);
  lastAdiid = rawAd5941Extract16(lastRawAdiid);
  lastChipid = rawAd5941Extract16(lastRawChipid);
  afeReady = (lastAdiid == AD5940_ADIID);

  Serial.print("#IDENTITY,raw_adiid32,0x");
  Serial.print(lastRawAdiid, HEX);
  Serial.print(",raw_chipid32,0x");
  Serial.print(lastRawChipid, HEX);
  Serial.print(",adiid,0x");
  Serial.print(lastAdiid, HEX);
  Serial.print(",chipid,0x");
  Serial.print(lastChipid, HEX);
  Serial.print(",ok,");
  Serial.println(afeReady ? "1" : "0");
  if (!afeReady) {
    printErrorDetail("AD5941_IDENTITY_FAILED", "remove_tf_module_or_check_power_spi_pins");
  }
  return afeReady;
}

static void selectWorkingElectrode(int ch) {
  ch = constrain(ch, 1, MAX_WE_CHANNELS);
  uint8_t index = (uint8_t)(ch - 1);
  digitalWrite(PIN_MUL_EN, LOW);
  digitalWrite(PIN_MUL_A0, (index & 0x01) ? HIGH : LOW);
  digitalWrite(PIN_MUL_A1, (index & 0x02) ? HIGH : LOW);
  delay(2);
  digitalWrite(PIN_MUL_EN, HIGH);
  delay(10);
}

static void disableWorkingElectrodeMux() {
  digitalWrite(PIN_MUL_EN, LOW);
  delay(2);
}

static String selectedWeText() {
  String text;
  for (uint8_t ch = 1; ch <= MAX_WE_CHANNELS; ch++) {
    if (!selectedWe[ch]) continue;
    if (text.length() > 0) text += ",";
    text += String(ch);
  }
  if (text.length() == 0) text = "-";
  return text;
}

static uint8_t selectedWeCount() {
  uint8_t count = 0;
  for (uint8_t ch = 1; ch <= MAX_WE_CHANNELS; ch++) {
    if (selectedWe[ch]) count++;
  }
  return count;
}

static void copyDpvMethodFields(DpvParams &dst, const DpvParams &src) {
  dst.weChannel = src.weChannel;
  dst.startMv = src.startMv;
  dst.endMv = src.endMv;
  dst.stepMv = src.stepMv;
  dst.pulseAmpMv = src.pulseAmpMv;
  dst.scanRateMvS = src.scanRateMvS;
  dst.pulseMs = src.pulseMs;
  dst.quietMs = src.quietMs;
  dst.stepSettleMs = src.stepSettleMs;
}

static void applyWeMethodToActive(uint8_t ch) {
  copyDpvMethodFields(params, weParams[ch]);
  params.weChannel = ch;
}

static bool parseWeList(const String &value) {
  bool newSelected[MAX_WE_CHANNELS + 1] = {false, false, false, false};
  String s = value;
  s.replace(";", ",");
  s.replace("|", ",");
  s.replace(" ", "");
  int start = 0;
  uint8_t count = 0;
  while (start < s.length()) {
    int comma = s.indexOf(',', start);
    String token = (comma < 0) ? s.substring(start) : s.substring(start, comma);
    token.trim();
    if (token.length() > 0) {
      int ch = token.toInt();
      if (ch < 1 || ch > (int)MAX_WE_CHANNELS) {
        setParamError("WE_LIST_RANGE", "wes_must_use_1_to_" + String(MAX_WE_CHANNELS));
        return false;
      }
      if (!newSelected[ch]) {
        newSelected[ch] = true;
        count++;
      }
    }
    if (comma < 0) break;
    start = comma + 1;
  }
  if (count == 0) {
    setParamError("WE_LIST_EMPTY", "select_at_least_one_we");
    return false;
  }
  for (uint8_t ch = 1; ch <= MAX_WE_CHANNELS; ch++) selectedWe[ch] = newSelected[ch];
  for (uint8_t ch = 1; ch <= MAX_WE_CHANNELS; ch++) {
    if (selectedWe[ch]) {
      params.weChannel = ch;
      applyWeMethodToActive(ch);
      break;
    }
  }
  return true;
}

static bool parseWeKey(const String &key, uint8_t *we, String *subkey) {
  if (!key.startsWith("we") || key.length() < 5) return false;
  char c = key.charAt(2);
  if (c < '1' || c > (char)('0' + MAX_WE_CHANNELS)) return false;
  char sep = key.charAt(3);
  if (sep != '.' && sep != '_') return false;
  *we = (uint8_t)(c - '0');
  *subkey = key.substring(4);
  return subkey->length() > 0;
}

static uint32_t rtiaSelectFromOhm(float ohm) {
  if (ohm <= 300) return LPTIARTIA_200R;
  if (ohm <= 1500) return LPTIARTIA_1K;
  if (ohm <= 2500) return LPTIARTIA_2K;
  if (ohm <= 3500) return LPTIARTIA_3K;
  if (ohm <= 5000) return LPTIARTIA_4K;
  if (ohm <= 7000) return LPTIARTIA_6K;
  if (ohm <= 9000) return LPTIARTIA_8K;
  if (ohm <= 11000) return LPTIARTIA_10K;
  if (ohm <= 14000) return LPTIARTIA_12K;
  if (ohm <= 18000) return LPTIARTIA_16K;
  if (ohm <= 22000) return LPTIARTIA_20K;
  if (ohm <= 27000) return LPTIARTIA_24K;
  if (ohm <= 31000) return LPTIARTIA_30K;
  if (ohm <= 36000) return LPTIARTIA_32K;
  if (ohm <= 44000) return LPTIARTIA_40K;
  if (ohm <= 56000) return LPTIARTIA_48K;
  if (ohm <= 75000) return LPTIARTIA_64K;
  if (ohm <= 90000) return LPTIARTIA_85K;
  if (ohm <= 98000) return LPTIARTIA_96K;
  if (ohm <= 110000) return LPTIARTIA_100K;
  if (ohm <= 124000) return LPTIARTIA_120K;
  if (ohm <= 144000) return LPTIARTIA_128K;
  if (ohm <= 178000) return LPTIARTIA_160K;
  if (ohm <= 226000) return LPTIARTIA_196K;
  if (ohm <= 384000) return LPTIARTIA_256K;
  return LPTIARTIA_512K;
}

static const char *rtiaSourceName(RtiaSource source) {
  switch (source) {
    case RTIA_SOURCE_INTERNAL: return "internal";
    case RTIA_SOURCE_EXT_AIN3_2K: return "ext_ain3_2k";
    case RTIA_SOURCE_EXT_AIN2_25K5: return "ext_ain2_25k5";
    case RTIA_SOURCE_EXT_AIN1_470K: return "ext_ain1_470k";
    default: return "unknown";
  }
}

static float rtiaEffectiveOhm() {
  switch (params.rtiaSource) {
    case RTIA_SOURCE_INTERNAL: return params.rtiaOhm;
    case RTIA_SOURCE_EXT_AIN3_2K: return 2000.0f;
    case RTIA_SOURCE_EXT_AIN2_25K5: return 25500.0f;
    case RTIA_SOURCE_EXT_AIN1_470K: return 470000.0f;
    default: return params.rtiaOhm;
  }
}

static bool rtiaSourceIsDiagnosticOnly() {
  return params.rtiaSource != RTIA_SOURCE_INTERNAL;
}

static bool parseRtiaSource(String raw, RtiaSource *source) {
  raw.trim();
  raw.toLowerCase();
  raw.replace(String("-"), String("_"));
  raw.replace(String(" "), String(""));
  if (raw == "0" || raw == "internal" || raw == "int") {
    *source = RTIA_SOURCE_INTERNAL;
    return true;
  }
  if (raw == "1" || raw == "ext_ain3_2k" || raw == "external_ain3_2k" || raw == "ain3_2k" || raw == "ext1") {
    *source = RTIA_SOURCE_EXT_AIN3_2K;
    return true;
  }
  if (raw == "2" || raw == "ext_ain2_25k5" || raw == "external_ain2_25k5" || raw == "ain2_25k5" || raw == "ain2_25.5k" || raw == "ext2") {
    *source = RTIA_SOURCE_EXT_AIN2_25K5;
    return true;
  }
  if (raw == "3" || raw == "ext_ain1_470k" || raw == "external_ain1_470k" || raw == "ain1_470k" || raw == "ext3") {
    *source = RTIA_SOURCE_EXT_AIN1_470K;
    return true;
  }
  setParamError("RTIA_SOURCE_UNKNOWN", "rtia_source_must_be_internal_ext_ain3_2k_ext_ain2_25k5_or_ext_ain1_470k");
  return false;
}

static uint32_t adcPgaSelect(float gain) {
  if (gain < 1.25f) return ADCPGA_1;
  if (gain < 1.75f) return ADCPGA_1P5;
  if (gain < 3.0f) return ADCPGA_2;
  if (gain < 6.0f) return ADCPGA_4;
  return ADCPGA_9;
}

static float clampSensorBiasMv(float biasMv) {
  float dac6 = (params.vzeroMv - 200.0f) / DAC6BITVOLT_1LSB;
  dac6 = clampf(dac6, 0.0f, 63.0f);
  float baseCode = floorf(dac6) * 64.0f;
  float minBias = -baseCode * DAC12BITVOLT_1LSB;
  float maxBias = (4095.0f - baseCode) * DAC12BITVOLT_1LSB;
  return clampf(biasMv, minBias, maxBias);
}

static void applyAmpConfig(float sensorBiasMv, bool redoRtiaCal) {
  AppAMPCfg_Type *cfg = nullptr;
  AppAMPGetCfg(&cfg);
  if (!cfg) return;
  sensorBiasMv = clampSensorBiasMv(sensorBiasMv);
  cfg->SeqStartAddr = 0;
  cfg->MaxSeqLen = 256;
  cfg->SeqStartAddrCal = 256;
  cfg->MaxSeqLenCal = 128;
  cfg->FifoThresh = 1;
  cfg->AmpODR = 0.02f;
  cfg->NumOfData = -1;
  cfg->RcalVal = params.rcalOhm;
  cfg->ADCRefVolt = params.adcRefVolt;
  cfg->ADCPgaGain = adcPgaSelect(params.adcPga);
  cfg->ADCSinc3Osr = ADCSINC3OSR_4;
  cfg->ADCSinc2Osr = ADCSINC2OSR_22;
  cfg->DataFifoSrc = FIFOSRC_SINC2NOTCH;
  if (params.rtiaSource == RTIA_SOURCE_INTERNAL) {
    cfg->ExtRtia = bFALSE;
    cfg->LptiaRtiaSel = rtiaSelectFromOhm(params.rtiaOhm);
    cfg->ExtRtiaVal = 0;
  } else {
    cfg->ExtRtia = bTRUE;
    cfg->LptiaRtiaSel = LPTIARTIA_OPEN;
    cfg->ExtRtiaVal = rtiaEffectiveOhm();
  }
  cfg->Vzero = params.vzeroMv;
  cfg->SensorBias = sensorBiasMv;
  cfg->ReDoRtiaCal = redoRtiaCal ? bTRUE : bFALSE;
  cfg->bParaChanged = bTRUE;
}

static void ensurePortReady() {
  if (portReady) return;
  keepTfDeselected();
  AD5940_Port_Init();
  keepTfDeselected();
  portReady = true;
}

static bool ensureAd5941Ready() {
  if (ad5940Initialized && afeReady) return true;
  afeInitTried = true;
  ensurePortReady();
  if (!afeReady) {
    if (!readAd5941Identity()) return false;
  }
  if (!ad5940Initialized) {
    printEvent("AD5940_INITIALIZE_BEGIN");
    AD5940_Initialize();
    ad5940Initialized = true;
    printEvent("AD5940_INITIALIZE_OK");
    readAd5941Identity();
  }
  return afeReady && ad5940Initialized;
}

static bool serviceSerialInput();

static bool waitWithStop(uint32_t ms) {
  uint32_t t0 = millis();
  while (millis() - t0 < ms) {
    serviceSerialInput();
    if (!runRequested) return false;
    delay(1);
  }
  return true;
}

static bool sampleCurrentAt(float sensorBiasMv, uint32_t dwellMs, float *currentUa) {
  applyAmpConfig(sensorBiasMv, false);
  AD5940Err err = AppAMPInit(seqBuffer, SEQ_BUFFER_WORDS);
  if (err != AD5940ERR_OK) {
    lastAfeError = err;
    Serial.print("#ERROR,APP_AMP_INIT,");
    Serial.println((int)err);
    rememberError("APP_AMP_INIT", "app_amp_init_failed_" + String((int)err));
    return false;
  }

  AppAMPCtrl(AMPCTRL_START, nullptr);
  if (!waitWithStop(dwellMs)) {
    AppAMPCtrl(AMPCTRL_STOPNOW, nullptr);
    return false;
  }

  float samples[8];
  uint32_t count = 0;
  uint32_t timeoutStart = millis();
  while (millis() - timeoutStart < params.sampleTimeoutMs) {
    count = 8;
    AD5940Err isrErr = AppAMPISR(samples, &count);
    if (isrErr == AD5940ERR_OK && count > 0) {
      float sum = 0.0f;
      for (uint32_t i = 0; i < count; i++) sum += samples[i];
      *currentUa = sum / (float)count;
      AppAMPCtrl(AMPCTRL_STOPNOW, nullptr);
      return true;
    }
    serviceSerialInput();
    if (!runRequested) {
      AppAMPCtrl(AMPCTRL_STOPNOW, nullptr);
      return false;
    }
    delay(2);
  }

  AppAMPCtrl(AMPCTRL_STOPNOW, nullptr);
  lastAfeError = -100;
  printErrorDetail("SAMPLE_TIMEOUT", "no_fifo_data_before_timeout");
  return false;
}

static void printRtiaDebug(const char *tag, uint8_t weChannel, uint16_t segment) {
  AppAMPCfg_Type *cfg = nullptr;
  AppAMPGetCfg(&cfg);
  if (!cfg) return;
  Serial.print("#INFO,rtia_debug,tag,");
  Serial.print(tag);
  Serial.print(",we,");
  Serial.print(weChannel);
  Serial.print(",segment,");
  Serial.print(segment);
  Serial.print(",rtia_source,");
  Serial.print(rtiaSourceName(params.rtiaSource));
  Serial.print(",rtia_selected_ohm,");
  Serial.print(rtiaEffectiveOhm(), 3);
  Serial.print(",rtia_calc_ohm,");
  Serial.println(cfg->RtiaCalValue.Magnitude, 3);
}

static void printParams() {
  Serial.print("#PARAM,wes,"); Serial.println(selectedWeText());
  Serial.print("#PARAM,active_we,"); Serial.println(params.weChannel);
  Serial.print("#PARAM,start_mV,"); Serial.println(params.startMv, 3);
  Serial.print("#PARAM,end_mV,"); Serial.println(params.endMv, 3);
  Serial.print("#PARAM,step_mV,"); Serial.println(params.stepMv, 3);
  Serial.print("#PARAM,pulse_amp_mV,"); Serial.println(params.pulseAmpMv, 3);
  Serial.print("#PARAM,scan_rate_mV_s,"); Serial.println(params.scanRateMvS, 3);
  Serial.print("#PARAM,pulse_ms,"); Serial.println(params.pulseMs);
  Serial.print("#PARAM,quiet_ms,"); Serial.println(params.quietMs);
  Serial.print("#PARAM,settle_ms,"); Serial.println(params.stepSettleMs);
  Serial.print("#PARAM,segments,"); Serial.println(weSegments[params.weChannel]);
  Serial.print("#PARAM,vzero_mV,"); Serial.println(params.vzeroMv, 3);
  Serial.print("#PARAM,rcal_ohm,"); Serial.println(params.rcalOhm, 3);
  Serial.print("#PARAM,rtia_source,"); Serial.println(rtiaSourceName(params.rtiaSource));
  Serial.print("#PARAM,rtia_internal_ohm,"); Serial.println(params.rtiaOhm, 3);
  Serial.print("#PARAM,rtia_effective_ohm,"); Serial.println(rtiaEffectiveOhm(), 3);
  Serial.print("#PARAM,rtia_usage,"); Serial.println(rtiaSourceIsDiagnosticOnly() ? "diagnostic_only" : "dpv_measurement");
  Serial.print("#PARAM,adc_ref_V,"); Serial.println(params.adcRefVolt, 6);
  Serial.print("#PARAM,adc_pga,"); Serial.println(params.adcPga, 3);
  Serial.print("#PARAM,timeout_ms,"); Serial.println(params.sampleTimeoutMs);
  Serial.print("#PARAM,max_points,"); Serial.println(params.maxPoints);
  for (uint8_t ch = 1; ch <= MAX_WE_CHANNELS; ch++) {
    Serial.print("#WE_PARAM,we,"); Serial.print(ch);
    Serial.print(",selected,"); Serial.print(selectedWe[ch] ? 1 : 0);
    Serial.print(",segments,"); Serial.print(weSegments[ch]);
    Serial.print(",start_mV,"); Serial.print(weParams[ch].startMv, 3);
    Serial.print(",end_mV,"); Serial.print(weParams[ch].endMv, 3);
    Serial.print(",step_mV,"); Serial.print(weParams[ch].stepMv, 3);
    Serial.print(",pulse_amp_mV,"); Serial.print(weParams[ch].pulseAmpMv, 3);
    Serial.print(",scan_rate_mV_s,"); Serial.print(weParams[ch].scanRateMvS, 3);
    Serial.print(",pulse_ms,"); Serial.print(weParams[ch].pulseMs);
    Serial.print(",quiet_ms,"); Serial.print(weParams[ch].quietMs);
    Serial.print(",settle_ms,"); Serial.println(weParams[ch].stepSettleMs);
  }
}

static void printStatus() {
  Serial.print("#STATUS,running,"); Serial.print(running ? 1 : 0);
  Serial.print(",requested,"); Serial.print(runRequested ? 1 : 0);
  Serial.print(",afe_ready,"); Serial.print(afeReady ? 1 : 0);
  Serial.print(",points,"); Serial.print(pointCount);
  Serial.print(",wes,"); Serial.print(selectedWeText());
  Serial.print(",last_afe_error,"); Serial.print(lastAfeError);
  Serial.print(",last_error_code,"); Serial.print(lastErrorCode);
  Serial.print(",last_error_message,"); Serial.println(lastErrorMessage);
}

static void printHelp() {
  Serial.println("#HELP,Commands:");
  Serial.println("#HELP,START");
  Serial.println("#HELP,STOP");
  Serial.println("#HELP,STATUS");
  Serial.println("#HELP,PARAM?");
  Serial.println("#HELP,ID?");
  Serial.println("#HELP,SET key value");
  Serial.println("#HELP,Single WE: SET we 1");
  Serial.println("#HELP,Multi WE: SET wes 1,2,3");
  Serial.println("#HELP,Common system keys: vzero rcal rtia rtia_source adc_ref adc_pga timeout_ms max_points");
  Serial.println("#HELP,RTIA source: SET rtia_source internal|ext_ain3_2k|ext_ain2_25k5|ext_ain1_470k");
  Serial.println("#HELP,External RTIA sources are diagnostic only. Use internal RTIA for normal DPV.");
  Serial.println("#HELP,DPV keys applied to all WE: start end step pulse_amp scan_rate pulse_ms quiet_ms settle_ms segments");
  Serial.println("#HELP,Per-WE DPV keys: we1.start we1.end we1.step we1.pulse_amp we1.scan_rate we1.pulse_ms we1.quiet_ms we1.settle_ms we1.segments");
  Serial.println("#HELP,DATA format: DATA,t_ms,we_channel,segment,index,e_base_mV,e_pulse_mV,i_base_uA,i_pulse_uA,dI_uA,valid");
  Serial.println("#HELP,Errors: #ERROR,code,message. Upper computer should show message and reject invalid user input.");
}

static bool setDpvMethodParam(DpvParams &target, uint16_t *segments, const String &key, float f) {
  if (key == "start") {
    if (!rangeOk(key, f, -1100.0f, 1100.0f)) return false;
    target.startMv = f;
  } else if (key == "end") {
    if (!rangeOk(key, f, -1100.0f, 1100.0f)) return false;
    target.endMv = f;
  } else if (key == "step") {
    if (!rangeOk(key, f, 1.0f, 100.0f)) return false;
    target.stepMv = f;
  } else if (key == "pulse_amp") {
    if (!rangeOk(key, f, -500.0f, 500.0f)) return false;
    target.pulseAmpMv = f;
  } else if (key == "scan_rate") {
    if (!rangeOk(key, f, 1.0f, 1000.0f)) return false;
    target.scanRateMvS = f;
  } else if (key == "pulse_ms") {
    if (!rangeOk(key, f, 5.0f, 5000.0f)) return false;
    target.pulseMs = (uint32_t)f;
  } else if (key == "quiet_ms") {
    if (!rangeOk(key, f, 0.0f, 60000.0f)) return false;
    target.quietMs = (uint32_t)f;
  } else if (key == "settle_ms") {
    if (!rangeOk(key, f, 0.0f, 10000.0f)) return false;
    target.stepSettleMs = (uint32_t)f;
  } else if (key == "segment" || key == "segments") {
    if (!rangeOk(key, f, 1.0f, 100.0f)) return false;
    *segments = (uint16_t)f;
  }
  else return false;
  return true;
}

static bool setParam(const String &key, const String &value) {
  clearParamError();
  float f = 0.0f;
  uint8_t targetWe = 0;
  String subkey;
  if (key == "wes") return parseWeList(value);
  if (key == "rtia_source") {
    RtiaSource source = RTIA_SOURCE_INTERNAL;
    if (!parseRtiaSource(value, &source)) return false;
    params.rtiaSource = source;
    return true;
  }
  if (!parseStrictFloat(value, &f)) {
    setParamError("PARAM_NOT_NUMBER", key + "_value_is_not_a_number");
    return false;
  }
  if (key == "we") {
    if (!rangeOk(key, f, 1.0f, (float)MAX_WE_CHANNELS)) return false;
    int ch = (int)f;
    for (uint8_t i = 1; i <= MAX_WE_CHANNELS; i++) selectedWe[i] = false;
    selectedWe[ch] = true;
    params.weChannel = ch;
    applyWeMethodToActive(ch);
    return true;
  }
  if (parseWeKey(key, &targetWe, &subkey)) {
    bool ok = setDpvMethodParam(weParams[targetWe], &weSegments[targetWe], subkey, f);
    if (ok && targetWe == params.weChannel) applyWeMethodToActive(targetWe);
    return ok;
  }
  if (key == "start" || key == "end" || key == "step" || key == "pulse_amp" ||
      key == "scan_rate" || key == "pulse_ms" || key == "quiet_ms" ||
      key == "settle_ms" || key == "segment" || key == "segments") {
    bool ok = true;
    for (uint8_t ch = 1; ch <= MAX_WE_CHANNELS; ch++) {
      ok = setDpvMethodParam(weParams[ch], &weSegments[ch], key, f) && ok;
    }
    applyWeMethodToActive(params.weChannel);
    return ok;
  }
  if (key == "vzero") {
    if (!rangeOk(key, f, 200.0f, 2200.0f)) return false;
    params.vzeroMv = f;
  } else if (key == "rcal") {
    if (!rangeOk(key, f, 1.0f, 10000000.0f)) return false;
    params.rcalOhm = f;
  } else if (key == "rtia") {
    if (!rangeOk(key, f, 200.0f, 512000.0f)) return false;
    params.rtiaOhm = f;
  } else if (key == "rtia_internal") {
    if (!rangeOk(key, f, 200.0f, 512000.0f)) return false;
    params.rtiaOhm = f;
  } else if (key == "adc_ref") {
    if (!rangeOk(key, f, 1.0f, 2.5f)) return false;
    params.adcRefVolt = f;
  } else if (key == "adc_pga") {
    if (!rangeOk(key, f, 1.0f, 9.0f)) return false;
    params.adcPga = f;
  } else if (key == "timeout_ms") {
    if (!rangeOk(key, f, 50.0f, 10000.0f)) return false;
    params.sampleTimeoutMs = (uint32_t)f;
  } else if (key == "max_points") {
    if (!rangeOk(key, f, 1.0f, 2000.0f)) return false;
    params.maxPoints = (uint16_t)f;
  } else {
    setParamError("UNKNOWN_PARAM", key + "_is_not_supported");
    return false;
  }
  return true;
}

static bool validateDpvParamsForWe(uint8_t ch) {
  const DpvParams &p = weParams[ch];
  String weName = String("we") + String(ch);
  if (fabsf(p.endMv - p.startMv) < 0.001f) {
    printErrorDetail("CONFIG_START_END_EQUAL", weName + "_start_and_end_are_equal");
    return false;
  }
  if (p.stepMv <= 0.0f) {
    printErrorDetail("CONFIG_BAD_STEP", weName + "_step_must_be_positive");
    return false;
  }
  float span = fabsf(p.endMv - p.startMv);
  uint32_t expectedPoints = (uint32_t)(floorf(span / p.stepMv) + 1.0f);
  if (expectedPoints > params.maxPoints) {
    printErrorDetail("CONFIG_MAX_POINTS_TOO_SMALL", weName + "_needs_" + String(expectedPoints) + "_points");
    return false;
  }
  if ((p.startMv + p.pulseAmpMv) < -1100.0f || (p.startMv + p.pulseAmpMv) > 1100.0f ||
      (p.endMv + p.pulseAmpMv) < -1100.0f || (p.endMv + p.pulseAmpMv) > 1100.0f) {
    printErrorDetail("CONFIG_PULSE_OUT_OF_RANGE", weName + "_pulse_potential_exceeds_safe_range");
    return false;
  }
  uint32_t pointMinMs = p.stepSettleMs + p.pulseMs + 5;
  uint32_t requestedPeriodMs = (uint32_t)((fabsf(p.stepMv) / p.scanRateMvS) * 1000.0f);
  if (requestedPeriodMs < pointMinMs) {
    Serial.print("#WARN,SCAN_RATE_LIMITED,we,");
    Serial.print(ch);
    Serial.print(",requested_period_ms,");
    Serial.print(requestedPeriodMs);
    Serial.print(",minimum_period_ms,");
    Serial.println(pointMinMs);
  }
  return true;
}

static bool validateConfigBeforeStart() {
  if (running) {
    printErrorDetail("ALREADY_RUNNING", "scan_is_already_running");
    return false;
  }
  if (selectedWeCount() == 0) {
    printErrorDetail("NO_WE_SELECTED", "select_at_least_one_we");
    return false;
  }
  if (!afeReady && afeInitTried) {
    printErrorDetail("AD5941_NOT_READY", "ad5941_identity_failed");
    return false;
  }
  for (uint8_t ch = 1; ch <= MAX_WE_CHANNELS; ch++) {
    if (!selectedWe[ch]) continue;
    if (!validateDpvParamsForWe(ch)) return false;
  }
  if (rtiaSourceIsDiagnosticOnly()) {
    Serial.print("#WARN,EXTERNAL_RTIA_DIAGNOSTIC_ONLY,source,");
    Serial.print(rtiaSourceName(params.rtiaSource));
    Serial.println(",message,use_internal_rtia_for_formal_dpv");
  }
  return true;
}

static void handleCommand(String line) {
  line.trim();
  if (line.length() == 0) return;
  line.replace('\t', ' ');
  while (line.indexOf("  ") >= 0) line.replace("  ", " ");

  String upper = line;
  upper.toUpperCase();

  if (upper == "HELP") {
    printHelp();
    return;
  }
  if (upper == "STATUS") {
    printStatus();
    return;
  }
  if (upper == "PARAM?") {
    printParams();
    return;
  }
  if (upper == "ID?") {
    readAd5941Identity();
    return;
  }
  if (upper == "START") {
    if (validateConfigBeforeStart()) {
      runRequested = true;
      printEvent("START_REQUESTED");
    }
    return;
  }
  if (upper == "STOP") {
    runRequested = false;
    AppAMPCtrl(AMPCTRL_STOPNOW, nullptr);
    printEvent("STOP_REQUESTED");
    return;
  }
  if (upper.startsWith("SET ")) {
    int firstSpace = line.indexOf(' ');
    int secondSpace = line.indexOf(' ', firstSpace + 1);
    if (secondSpace < 0) {
      printErrorDetail("SET_FORMAT", "use_SET_key_value");
      return;
    }
    String key = line.substring(firstSpace + 1, secondSpace);
    String value = line.substring(secondSpace + 1);
    key.trim();
    value.trim();
    key.toLowerCase();
    if (running) {
      printError("CANNOT_SET_WHILE_RUNNING");
      return;
    }
    if (setParam(key, value)) {
      Serial.print("#OK,SET,");
      Serial.print(key);
      Serial.print(",");
      Serial.println(value);
    } else {
      if (paramErrorCode.length() == 0) setParamError("UNKNOWN_PARAM", key + "_is_not_supported");
      printErrorDetail(paramErrorCode, paramErrorMessage);
    }
    return;
  }

  printErrorDetail("UNKNOWN_COMMAND", "command_not_supported");
}

static bool serviceSerialInput() {
  bool handled = false;
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      handleCommand(serialLine);
      serialLine = "";
      handled = true;
    } else if (serialLine.length() < 120) {
      serialLine += c;
    } else {
      serialLine = "";
      printErrorDetail("LINE_TOO_LONG", "serial_command_over_120_chars");
    }
  }
  return handled;
}

static void outputDataPoint(const DpvPoint &p) {
  bool valid = isfinite(p.iBaseUa) && isfinite(p.iPulseUa) && isfinite(p.dIUa);
  Serial.print("DATA,");
  Serial.print(p.tMs);
  Serial.print(",");
  Serial.print(p.weChannel);
  Serial.print(",");
  Serial.print(p.segment);
  Serial.print(",");
  Serial.print(p.index);
  Serial.print(",");
  Serial.print(finiteText(p.eBaseMv, 3));
  Serial.print(",");
  Serial.print(finiteText(p.ePulseMv, 3));
  Serial.print(",");
  Serial.print(finiteText(p.iBaseUa, 6));
  Serial.print(",");
  Serial.print(finiteText(p.iPulseUa, 6));
  Serial.print(",");
  Serial.print(finiteText(p.dIUa, 6));
  Serial.print(",");
  Serial.println(valid ? 1 : 0);
}

static bool prepareWeSegment(uint8_t weChannel, uint16_t segment, DpvRunState *state) {
  applyWeMethodToActive(weChannel);
  selectWorkingElectrode(weChannel);
  Serial.print("#EVENT,WE_BEGIN,we,");
  Serial.print(weChannel);
  Serial.print(",segment,");
  Serial.println(segment);
  applyAmpConfig(params.startMv, true);
  AD5940Err initErr = AppAMPInit(seqBuffer, SEQ_BUFFER_WORDS);
  if (initErr != AD5940ERR_OK) {
    lastAfeError = initErr;
    Serial.print("#ERROR,FIRST_APP_AMP_INIT,");
    Serial.println((int)initErr);
    rememberError("FIRST_APP_AMP_INIT", "first_app_amp_init_failed_" + String((int)initErr));
    return false;
  }
  printRtiaDebug("segment_init", weChannel, segment);

  if (!waitWithStop(params.quietMs)) {
    printEvent("SEGMENT_STOPPED_DURING_QUIET");
    return false;
  }

  float dir = (params.endMv >= params.startMv) ? 1.0f : -1.0f;
  state->active = true;
  state->done = false;
  state->index = 0;
  state->points = 0;
  state->eBaseMv = params.startMv;
  state->stepMv = fabsf(params.stepMv) * dir;
  state->stepPeriodMs = (uint32_t)((fabsf(params.stepMv) / params.scanRateMvS) * 1000.0f);
  uint32_t minPeriod = params.stepSettleMs + params.pulseMs + 5;
  if (state->stepPeriodMs < minPeriod) state->stepPeriodMs = minPeriod;
  state->nextDueMs = 0;
  return true;
}

static bool runOneDpvPoint(uint8_t weChannel, uint16_t segment, DpvRunState *state) {
  if (!state->active || state->done) return true;
  applyWeMethodToActive(weChannel);
  bool beyond = (state->stepMv > 0) ? (state->eBaseMv > params.endMv + 0.001f) : (state->eBaseMv < params.endMv - 0.001f);
  if (beyond || state->index >= params.maxPoints) {
    state->done = true;
    return true;
  }

  selectWorkingElectrode(weChannel);
  uint32_t pointStart = millis();
  float iBase = NAN;
  float iPulse = NAN;
  if (!sampleCurrentAt(state->eBaseMv, params.stepSettleMs, &iBase)) return false;

  float ePulse = state->eBaseMv + params.pulseAmpMv;
  if (!sampleCurrentAt(ePulse, params.pulseMs, &iPulse)) return false;

  DpvPoint p;
  p.tMs = millis();
  p.segment = segment;
  p.index = state->index;
  p.weChannel = weChannel;
  p.eBaseMv = state->eBaseMv;
  p.ePulseMv = ePulse;
  p.iBaseUa = iBase;
  p.iPulseUa = iPulse;
  p.dIUa = iPulse - iBase;
  outputDataPoint(p);
  pointCount++;
  state->points++;
  state->index++;
  state->eBaseMv += state->stepMv;
  state->nextDueMs = pointStart + state->stepPeriodMs;
  return true;
}

static void printWeSegmentEnd(uint8_t weChannel, uint16_t segment, const DpvRunState &state) {
  Serial.print("#EVENT,WE_END,we,");
  Serial.print(weChannel);
  Serial.print(",segment,");
  Serial.print(segment);
  Serial.print(",points,");
  Serial.print(state.points);
  Serial.print(",points_total,");
  Serial.println(pointCount);
}

static bool runRoundRobinSegment(uint16_t segment) {
  DpvRunState states[MAX_WE_CHANNELS + 1];
  for (uint8_t ch = 0; ch <= MAX_WE_CHANNELS; ch++) {
    states[ch].active = false;
    states[ch].done = true;
    states[ch].index = 0;
    states[ch].points = 0;
    states[ch].eBaseMv = 0;
    states[ch].stepMv = 0;
    states[ch].stepPeriodMs = 0;
    states[ch].nextDueMs = 0;
  }

  uint8_t activeCount = 0;
  for (uint8_t ch = 1; runRequested && ch <= MAX_WE_CHANNELS; ch++) {
    if (!selectedWe[ch] || segment > weSegments[ch]) continue;
    if (!prepareWeSegment(ch, segment, &states[ch])) return false;
    activeCount++;
  }
  if (activeCount == 0) return true;

  while (runRequested) {
    bool anyAlive = false;
    bool anyRan = false;
    uint32_t now = millis();
    for (uint8_t ch = 1; runRequested && ch <= MAX_WE_CHANNELS; ch++) {
      if (!states[ch].active || states[ch].done) continue;
      anyAlive = true;
      if (states[ch].nextDueMs != 0 && (int32_t)(now - states[ch].nextDueMs) < 0) continue;
      if (!runOneDpvPoint(ch, segment, &states[ch])) return false;
      anyRan = true;
    }
    if (!anyAlive) break;
    serviceSerialInput();
    if (!anyRan) delay(1);
  }

  for (uint8_t ch = 1; ch <= MAX_WE_CHANNELS; ch++) {
    if (states[ch].active) printWeSegmentEnd(ch, segment, states[ch]);
  }
  return runRequested;
}

static void runDpvScan() {
  if (!ensureAd5941Ready()) {
    runRequested = false;
    printErrorDetail("AD5941_NOT_READY", "ad5941_not_ready_at_start");
    return;
  }

  if (selectedWeCount() == 0) {
    runRequested = false;
    printErrorDetail("NO_WE_SELECTED", "select_at_least_one_we");
    return;
  }

  running = true;
  pointCount = 0;
  lastAfeError = 0;

  Serial.print("#EVENT,BATCH_BEGIN,we_count,");
  Serial.print(selectedWeCount());
  Serial.print(",wes,");
  Serial.println(selectedWeText());
  printParams();
  Serial.println("#HEADER,t_ms,we_channel,segment,index,e_base_mV,e_pulse_mV,i_base_uA,i_pulse_uA,dI_uA,valid");

  uint16_t maxSegments = 0;
  for (uint8_t ch = 1; ch <= MAX_WE_CHANNELS; ch++) {
    if (selectedWe[ch] && weSegments[ch] > maxSegments) maxSegments = weSegments[ch];
  }
  for (uint16_t seg = 1; runRequested && seg <= maxSegments; seg++) {
    Serial.print("#EVENT,SEGMENT_BEGIN,segment,");
    Serial.println(seg);
    if (!runRoundRobinSegment(seg)) {
      runRequested = false;
      break;
    }
    Serial.print("#EVENT,SEGMENT_END,segment,");
    Serial.print(seg);
    Serial.print(",points_total,");
    Serial.println(pointCount);
  }

  AppAMPCtrl(AMPCTRL_STOPNOW, nullptr);
  disableWorkingElectrodeMux();
  running = false;
  runRequested = false;
  Serial.print("#EVENT,BATCH_END,points,");
  Serial.println(pointCount);
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(1000);

  keepTfDeselected();
  pinMode(PIN_AD5941_CS, OUTPUT);
  digitalWrite(PIN_AD5941_CS, HIGH);
  pinMode(PIN_MUL_EN, OUTPUT);
  pinMode(PIN_MUL_A0, OUTPUT);
  pinMode(PIN_MUL_A1, OUTPUT);
  disableWorkingElectrodeMux();
  for (uint8_t ch = 1; ch <= MAX_WE_CHANNELS; ch++) {
    weParams[ch] = params;
    weParams[ch].weChannel = ch;
    weSegments[ch] = DEFAULT_SEGMENTS;
  }
  applyWeMethodToActive(1);

  Serial.println("#EVENT,BOOT");
  Serial.print("#INFO,firmware,AD5941_DPV_SERIAL_UPLINK");
  Serial.print(",baud,");
  Serial.println(SERIAL_BAUD);
  esp_reset_reason_t rst = esp_reset_reason();
  Serial.print("#INFO,reset,");
  Serial.print(resetReasonText(rst));
  Serial.print(",");
  Serial.println((int)rst);
  Serial.println("#INFO,storage,TF_DISABLED_SHARED_MISO_CONFLICT");
  Serial.println("#INFO,wifi,DISABLED_USE_UPPER_COMPUTER");
  Serial.println("#INFO,serial,Send HELP for commands");
  printParams();

  ensurePortReady();
  readAd5941Identity();
}

void loop() {
  serviceSerialInput();
  if (runRequested && !running) {
    runDpvScan();
  }
  keepTfDeselected();
  delay(1);
}
