/**
 * @file       TinyGsmClientN716.h
 * @author     Volodymyr Shymanskyy
 * @license    LGPL-3.0
 * @copyright  Copyright (c) 2016 Volodymyr Shymanskyy
 * @date       Apr 2018
 */

#ifndef SRC_TINYGSMCLIENTN716_H_
#define SRC_TINYGSMCLIENTN716_H_
// #pragma message("TinyGSM:  TinyGsmClientN716")

// #define TINY_GSM_DEBUG Serial

#define TINY_GSM_MUX_COUNT 6
//#define TINY_GSM_BUFFER_READ_AND_CHECK_SIZE
#define TINY_GSM_BUFFER_READ_NO_CHECK

#include "TinyGsmBattery.tpp"
#include "TinyGsmCalling.tpp"
#include "TinyGsmGPRS.tpp"
#include "TinyGsmGPS.tpp"
#include "TinyGsmModem.tpp"
#include "TinyGsmSMS.tpp"
#include "TinyGsmTCP.tpp"
#include "TinyGsmTemperature.tpp"
#include "TinyGsmTime.tpp"
#include "TinyGsmNTP.tpp"

#define GSM_NL "\r\n"
static const char GSM_OK[] TINY_GSM_PROGMEM    = "OK" GSM_NL;
static const char GSM_ERROR[] TINY_GSM_PROGMEM = "ERROR" GSM_NL;
#if defined       TINY_GSM_DEBUG
static const char GSM_CME_ERROR[] TINY_GSM_PROGMEM = GSM_NL "+CME ERROR:";
static const char GSM_CMS_ERROR[] TINY_GSM_PROGMEM = GSM_NL "+CMS ERROR:";
#endif

enum RegStatus {
  REG_NO_RESULT    = -1,
  REG_UNREGISTERED = 0,
  REG_SEARCHING    = 2,
  REG_DENIED       = 3,
  REG_OK_HOME      = 1,
  REG_OK_ROAMING   = 5,
  REG_UNKNOWN      = 4,
};

class TinyGsmN716 : public TinyGsmModem<TinyGsmN716>,
                    public TinyGsmGPRS<TinyGsmN716>,
                    public TinyGsmTCP<TinyGsmN716, TINY_GSM_MUX_COUNT>,
                    public TinyGsmCalling<TinyGsmN716>,
                    public TinyGsmSMS<TinyGsmN716>,
                    public TinyGsmTime<TinyGsmN716>,
                    public TinyGsmNTP<TinyGsmN716>,
                    public TinyGsmGPS<TinyGsmN716>,
                    public TinyGsmBattery<TinyGsmN716>,
                    public TinyGsmTemperature<TinyGsmN716> {
  friend class TinyGsmModem<TinyGsmN716>;
  friend class TinyGsmGPRS<TinyGsmN716>;
  friend class TinyGsmTCP<TinyGsmN716, TINY_GSM_MUX_COUNT>;
  friend class TinyGsmCalling<TinyGsmN716>;
  friend class TinyGsmSMS<TinyGsmN716>;
  friend class TinyGsmTime<TinyGsmN716>;
  friend class TinyGsmNTP<TinyGsmN716>;
  friend class TinyGsmGPS<TinyGsmN716>;
  friend class TinyGsmBattery<TinyGsmN716>;
  friend class TinyGsmTemperature<TinyGsmN716>;

  /*
   * Inner Client
   */
 public:
  class GsmClientN716 : public GsmClient {
    friend class TinyGsmN716;

   public:
    GsmClientN716() {}

    explicit GsmClientN716(TinyGsmN716& modem, uint8_t mux = 0) {
      init(&modem, mux);
    }

    bool init(TinyGsmN716* modem, uint8_t mux = 0) {
      this->at       = modem;
      sock_available = 0;
      prev_check     = 0;
      sock_connected = false;
      got_data       = false;

      if (mux < TINY_GSM_MUX_COUNT) {
        this->mux = mux;
      } else {
        this->mux = (mux % TINY_GSM_MUX_COUNT);
      }
      at->sockets[this->mux] = this;

      return true;
    }

   public:
    virtual int connect(const char* host, uint16_t port, int timeout_s) {
      stop();
      TINY_GSM_YIELD();
      rx.clear();
      sock_connected = at->modemConnect(host, port, mux, false, timeout_s);
      return sock_connected;
    }
    TINY_GSM_CLIENT_CONNECT_OVERRIDES

    void stop(uint32_t maxWaitMs) {
      uint32_t startMillis = millis();
      dumpModemBuffer(maxWaitMs);
      at->sendAT(GF("+TCPCLOSE="), mux);
      sock_connected = false;
      at->waitResponse((maxWaitMs - (millis() - startMillis)));
    }
    void stop() override {
      stop(15000L);
    }

    /*
     * Extended API
     */

    String remoteIP() TINY_GSM_ATTR_NOT_IMPLEMENTED;
  };

  /*
   * Inner Secure Client
   */

  /*
  class GsmClientSecureN716 : public GsmClientN716
  {
  public:
    GsmClientSecure() {}

    GsmClientSecure(TinyGsmN716& modem, uint8_t mux = 0)
     : public GsmClient(modem, mux)
    {}


  public:
    int connect(const char* host, uint16_t port, int timeout_s) override {
      stop();
      TINY_GSM_YIELD();
      rx.clear();
      sock_connected = at->modemConnect(host, port, mux, true, timeout_s);
      return sock_connected;
    }
    TINY_GSM_CLIENT_CONNECT_OVERRIDES
  };
  */

  /*
   * Constructor
   */
 public:
  explicit TinyGsmN716(Stream& stream) : stream(stream) {
    memset(sockets, 0, sizeof(sockets));
  }

  /*
   * Basic functions
   */
 protected:
  bool initImpl(const char* pin = NULL) {
    DBG(GF("### TinyGSM Version:"), TINYGSM_VERSION);
    DBG(GF("### TinyGSM Compiled Module:  TinyGsmClientN716"));

    if (!testAT()) { return false; }

    sendAT(GF("E0"));  // Echo Off
    if (waitResponse() != 1) { return false; }

#ifdef TINY_GSM_DEBUG
    sendAT(GF("+CMEE=2"));  // turn on verbose error codes
#else
    sendAT(GF("+CMEE=0"));  // turn off error codes
#endif
    waitResponse();

    sendAT(GF("+RECVMODE=0"));  // 缓存数据
    if (waitResponse() != 1) { return false; } 

    DBG(GF("### Modem:"), getModemName());

    // Disable time and time zone URC's
    // sendAT(GF("+CTZR=0"));
    // if (waitResponse(10000L) != 1) { return false; }

    // Enable automatic time zone update
    // sendAT(GF("+CTZU=1"));
    // if (waitResponse(10000L) != 1) { return false; }

    SimStatus ret = getSimStatus();
    // if the sim isn't ready and a pin has been provided, try to unlock the sim
    if (ret != SIM_READY && pin != NULL && strlen(pin) > 0) {
      simUnlock(pin);
      return (getSimStatus() == SIM_READY);
    } else {
      // if the sim is ready, or it's locked but no pin has been provided,
      // return true
      return (ret == SIM_READY || ret == SIM_LOCKED);
    }
  }

  /*
   * Power functions
   */
 protected:
  bool restartImpl(const char* pin = NULL) {
    if (!testAT()) { return false; }
    if (!setPhoneFunctionality(1, true)) { return false; }
    waitResponse(10000L, GF("+PBREADY"));//!!!!!!!!!!!!!!!!!!!!(TODO)
    return init(pin);
  }

  bool powerOffImpl() {
    sendAT(GF("+QPOWD=1"));//!!!!!!!!!!!!!!!!!!!!!!!!!(TODO)
    waitResponse(300);  // returns OK first
    return waitResponse(300, GF("POWERED DOWN")) == 1;
  }

  // When entering into sleep mode is enabled, DTR is pulled up, and WAKEUP_IN
  // is pulled up, the module can directly enter into sleep mode.If entering
  // into sleep mode is enabled, DTR is pulled down, and WAKEUP_IN is pulled
  // down, there is a need to pull the DTR pin and the WAKEUP_IN pin up first,
  // and then the module can enter into sleep mode.
  bool sleepEnableImpl(bool enable = true) {
    sendAT(GF("+ENPWRSAVE="), enable);
    return waitResponse() == 1;
  }

  bool setPhoneFunctionalityImpl(uint8_t fun, bool reset = false) {
    sendAT(GF("+CFUN="), fun, reset ? ",1" : "");
    return waitResponse(10000L, GF("OK")) == 1;
  }

  /*
   * Generic network functions
   */
 public:
  RegStatus getRegistrationStatus() {
    // Check first for EPS registration
    RegStatus epsStatus = (RegStatus)getRegistrationStatusXREG("CEREG");

    // If we're connected on EPS, great!
    if (epsStatus == REG_OK_HOME || epsStatus == REG_OK_ROAMING) {
      return epsStatus;
    } else {
      // Otherwise, check generic network status
      return (RegStatus)getRegistrationStatusXREG("CREG");
    }
  }

 protected:
  bool isNetworkConnectedImpl() {
    RegStatus s = getRegistrationStatus();
    return (s == REG_OK_HOME || s == REG_OK_ROAMING);
  }

  /*
   * GPRS functions
   */
 protected:
  bool gprsConnectImpl(const char* apn, const char* user = NULL,
                       const char* pwd = NULL) {
    gprsDisconnect();

    // Configure the TCPIP Context//AT+CGDCONT=1,"IP","UNINET"
    sendAT(GF("+CGDCONT=1,\"IP\",\""), apn, GF("\""));
    if (waitResponse() != 1) { return false; }
    //AT+XGAUTH=1,1, "CARD","CARD"
    sendAT(GF("+XGAUTH=1,1,\""), user, GF("\",\""), pwd,GF("\""));

    if (waitResponse() != 1) { return false; }
        // Attach to Packet Domain service - is this necessary?
    sendAT(GF("+CGATT=1"));
    if (waitResponse(60000L) != 1) { return false; }

    // Activate PPP CONNECT
    sendAT(GF("+XIIC=1"));
    if (waitResponse(150000L) != 1) { return false; }

    return true;
  }

  bool gprsDisconnectImpl() {
    sendAT(GF("+XIIC=0"));  // Deactivate the bearer context
    if (waitResponse(40000L) != 1) { return false; }

    return true;
  }

  /*
   * SIM card functions
   */
 protected:
  String getSimCCIDImpl() {
    sendAT(GF("+CCID"));
    if (waitResponse(GF(GSM_NL "+CCID:")) != 1) { return ""; }
    String res = stream.readStringUntil('\n');
    waitResponse();
    res.trim();
    return res;
  }

  /*
   * Phone Call functions
   */
 protected:
  // Can follow all of the phone call functions from the template

  /*
   * Messaging functions
   */
 protected:
  // Follows all messaging functions per template

  /*
   * GSM Location functions
   */
 protected:
  // NOTE:  As of application firmware version 01.016.01.016 triangulated
  // locations can be obtained via the QuecLocator service and accompanying AT
  // commands.  As this is a separate paid service which I do not have access
  // to, I am not implementing it here.

  /*
   * GPS/GNSS/GLONASS location functions
   */
 protected:
  // enable GPS
  bool enableGPSImpl() {
    sendAT(GF("$MYGPSPWR=1"));
    if (waitResponse() != 1) { return false; }
    return true;
  }

  bool disableGPSImpl() {
    sendAT(GF("$MYGPSPWR=0"));
    if (waitResponse() != 1) { return false; }
    return true;
  }

  // get the RAW GPS output
  String getGPSrawImpl() {
    sendAT(GF("+CIPGSMLOC=1"));
    if (waitResponse(10000L, GF(GSM_NL "+CIPGSMLOC:")) != 1) { return ""; }
    String res = stream.readStringUntil('\n');
    waitResponse();
    res.trim();
    return res;
  }

  // get GPS informations
  bool getGPSImpl(float* lat, float* lon, float* speed = 0, float* alt = 0,
                  int* vsat = 0, int* usat = 0, float* accuracy = 0,
                  int* year = 0, int* month = 0, int* day = 0, int* hour = 0,
                  int* minute = 0, int* second = 0) {
    sendAT(GF("+CIPGSMLOC=1"));
    //+CIPGSMLOC: {"location":{"lat":22.69083,"lng":113.985228}, "accuracy":0.0}
    if (waitResponse(10000L, GF(GSM_NL "+CIPGSMLOC:")) != 1) {
      // NOTE:  Will return an error if the position isn't fixed
      return false;
    }
    if (waitResponse(100L, GF("location")) != 1) {
      // NOTE:  Will return an error if the position isn't fixed
      return false;
    }

    // init variables
    float ilat         = 0;
    float ilon         = 0;
    float ispeed       = 0;
    float ialt         = 0;
    int   iusat        = 0;
    float iaccuracy    = 0;
    int   iyear        = 0;
    int   imonth       = 0;
    int   iday         = 0;
    int   ihour        = 0;
    int   imin         = 0;
    int   isec         = 0;
     
    // UTC date & Time  
    streamSkipUntil(':');
    streamSkipUntil(':');
    ilat      = streamGetFloatBefore(',');  // Latitude
    streamSkipUntil(':');
    ilon      = streamGetFloatBefore('}');  // Longitude
    streamSkipUntil(':');
    iaccuracy = streamGetFloatBefore('}');  //
    
    streamSkipUntil('\n');  // The error code of the operation. If it is not
                            // 0, it is the type of error.
    sendAT(GF("+CCLK?"));
    if (waitResponse(2000L, GF("+CCLK: \"")) != 1) { return ""; }

    // Date & Time
    iyear       = streamGetIntBefore('/');
    imonth      = streamGetIntBefore('/');
    iday        = streamGetIntBefore(',');
    ihour       = streamGetIntBefore(':');
    imin        = streamGetIntBefore(':');
    isec        = streamGetIntLength(2);
    // char tzSign = stream.read();
    // itimezone   = streamGetIntBefore('"');
    // if (tzSign == '-') { itimezone = itimezone * -1; }
    streamSkipUntil('\n');  // DST flag

    // Set pointers
    if (lat != NULL) *lat = ilat;
    if (lon != NULL) *lon = ilon;
    if (speed != NULL) *speed = ispeed;
    if (alt != NULL) *alt = ialt;
    if (vsat != NULL) *vsat = 0;
    if (usat != NULL) *usat = iusat;
    if (accuracy != NULL) *accuracy = iaccuracy;
    if (iyear < 2000) iyear += 2000;
    if (year != NULL) *year = iyear;
    if (month != NULL) *month = imonth;
    if (day != NULL) *day = iday;
    if (hour != NULL) *hour = ihour;
    if (minute != NULL) *minute = imin;
    if (second != NULL) *second = isec;

    waitResponse();  // Final OK
    return true;
  }

  /*
   * Time functions
   */
 protected:
  String getGSMDateTimeImpl(TinyGSMDateTimeFormat format) {
    sendAT(GF("+CCLK?"));
    if (waitResponse(2000L, GF("+CCLK: \"")) != 1) { return ""; }

    String res;

    switch (format) {
      case DATE_FULL: res = stream.readStringUntil('"'); break;
      case DATE_TIME:
        streamSkipUntil(',');
        res = stream.readStringUntil('"');
        break;
      case DATE_DATE: res = stream.readStringUntil(','); break;
    }
    waitResponse();  // Ends with OK
    return res;
  }

  // The N716 returns UTC time instead of local time as other modules do in
  // response to CCLK, so we're using QLTS where we can specifically request
  // local time.
  bool getNetworkTimeImpl(int* year, int* month, int* day, int* hour,
                          int* minute, int* second, float* timezone) {
    sendAT(GF("+CCLK?"));
    if (waitResponse(2000L, GF("+CCLK: \"")) != 1) { return false; }

    int iyear     = 0;
    int imonth    = 0;
    int iday      = 0;
    int ihour     = 0;
    int imin      = 0;
    int isec      = 0;
    int itimezone = 0;

    // Date & Time
    iyear       = streamGetIntBefore('/');
    imonth      = streamGetIntBefore('/');
    iday        = streamGetIntBefore(',');
    ihour       = streamGetIntBefore(':');
    imin        = streamGetIntBefore(':');
    isec        = streamGetIntLength(2);
    char tzSign = stream.read();
    itimezone   = streamGetIntBefore('"');
    if (tzSign == '-') { itimezone = itimezone * -1; }
    streamSkipUntil('\n');  // DST flag

    // Set pointers
    if (iyear < 2000) iyear += 2000;
    if (year != NULL) *year = iyear;
    if (month != NULL) *month = imonth;
    if (day != NULL) *day = iday;
    if (hour != NULL) *hour = ihour;
    if (minute != NULL) *minute = imin;
    if (second != NULL) *second = isec;
    if (timezone != NULL) *timezone = static_cast<float>(itimezone) / 4.0;//使用1/4时区

    // Final OK
    waitResponse();  // Ends with OK
    return true;
  }

  /*
   * NTP server functions
   */

  byte NTPServerSyncImpl(String server = "pool.ntp.org", byte = -5) {
    // Request network synchronization
    // AT+UPDATETIME=<mode>[,<serv_ip>,<time>[[,<TZ>][,<DST>]]]<CR> 
    sendAT(GF("+UPDATETIME=1,\""), server, '\",10');
    if (waitResponse(10000L, GF("+UPDATETIME: Update To"))) {
      String result = stream.readStringUntil(',');
      streamSkipUntil('\n');
      result.trim();
      if (TinyGsmIsValidNumber(result)) { return result.toInt(); }
    } else {
      return -1;
    }
    return -1;
  }

  String ShowNTPErrorImpl(byte error) TINY_GSM_ATTR_NOT_IMPLEMENTED;

  /*
   * Battery functions
   */
 protected:
  // Can follow CBC as in the template

  /*
   * Temperature functions
   */
 protected:
  // get temperature in degree celsius
  uint16_t getTemperatureImpl() {
    sendAT(GF("+QTEMP"));
    if (waitResponse(GF(GSM_NL "+QTEMP:")) != 1) { return 0; }
    // return temperature in C
    uint16_t res =
        streamGetIntBefore(',');  // read PMIC (primary ic) temperature
    streamSkipUntil(',');         // skip XO temperature ??
    streamSkipUntil('\n');        // skip PA temperature ??
    // Wait for final OK
    waitResponse();
    return res;
  }

  /*
   * Client related functions
   */
 protected:
  bool modemConnect(const char* host, uint16_t port, uint8_t mux,
                    bool ssl = false, int timeout_s = 150) {
    if (ssl) { DBG("SSL not yet supported on this module!"); }

    uint32_t timeout_ms = ((uint32_t)timeout_s) * 1000;

    // <PDPcontextID>(1-16), <connectID>(0-11),
    // "TCP/UDP/TCP LISTENER/UDPSERVICE", "<IP_address>/<domain_name>",
    // <remote_port>,<local_port>,<access_mode>(0-2; 0=buffer)
    sendAT(GF("+TCPSETUP="), mux,GF(","), host, GF(","),port);
    waitResponse();

    if (waitResponse(timeout_ms, GF("+TCPSETUP: ")) != 1) { return false; }
    if (streamGetIntBefore(',') != mux) { return false; }
    // Read status
    
    if(waitResponse(GF("OK"), GF("FAIL"), GF("ERROR1")) == 1) { 
      DBG("### Connect: success","on", mux);
      sendAT(GF("+IPSTATUS="), mux);
      if (waitResponse(timeout_ms, GF("+IPSTATUS: ")) != 1) { return false; }
      if (streamGetIntBefore(',') != mux) { return false; }
      if(waitResponse(GF("CONNECT"), GF("DISCONNECT")) == 1) { 
        DBG("### Connect: success","on", mux);
        streamSkipUntil('\n');
        return true;
      }
    }
    return false;
  }

  int16_t modemSend(const void* buff, size_t len, uint8_t mux) {
    //sendAT(GF("+TCPSEND="), mux);
    //if (waitResponse(GF("+TCPACK: ")) != 1) { return false; }
    sendAT(GF("+TCPSEND="), mux, ',', (uint16_t)len);
    if (waitResponse(GF(">")) != 1) { return 0; }
    stream.write(reinterpret_cast<const uint8_t*>(buff), len);
    stream.flush();
    if (waitResponse(GF(GSM_NL "+TCPSEND: ")) != 1) { return 0; }
    if (streamGetIntBefore(',') != mux) { return false; }
    if (streamGetIntBefore('\n') != len) { return false; }
    // TODO(?): Wait for ACK? +TCPSEND: 0,1
    return len;
  }

  size_t modemRead(size_t size, uint8_t mux) {
    if (!sockets[mux]) return 0;
    sendAT(GF("+TCPREAD="), mux, ',',(uint16_t)size);//
    if (waitResponse(GF("+TCPREAD:"), GFP(GSM_OK), GFP(GSM_ERROR)) == 1) { 
      if (streamGetIntBefore(',') != mux) { return 0; }

      int16_t len = streamGetIntBefore(',');
      if (len < size) { sockets[mux]->sock_available = len; }
      for (int i = 0; i < len; i++) { 
        moveCharFromStreamToFifo(mux); 
        sockets[mux]->sock_available--;
      }
      waitResponse();
      DBG("### READ:", len, "from", mux);
      return len;
    }else {
      sockets[mux]->sock_available = 0;
      
      return 0;
    }
  }

  size_t modemGetAvailable(uint8_t mux) {
    return 0;
  }

  bool modemGetConnected(uint8_t mux) {
    char tmp[11] = "";
    int8_t  res = 4;
    sendAT(GF("+IPSTATUS="), mux);
    // +QISTATE: 0,"TCP","151.139.237.11",80,5087,4,1,0,0,"uart1"

    if (waitResponse(GF("+IPSTATUS:")) != 1) { return false; }

    streamSkipUntil(',');                  // Skip mux
    res = waitResponse(GF("DISCONNECT"),GF("CONNECT"));
    streamSkipUntil('\n');
    //waitResponse();

    // 0 Initial, 1 Opening, 2 Connected, 3 Listening, 4 Closing
    return 2 == res;
  }

  /*
   * Utilities
   */
 public:
  // TODO(vshymanskyy): Optimize this!
  int8_t waitResponse(uint32_t timeout_ms, String& data,
                      GsmConstStr r1 = GFP(GSM_OK),
                      GsmConstStr r2 = GFP(GSM_ERROR),
#if defined TINY_GSM_DEBUG
                      GsmConstStr r3 = GFP(GSM_CME_ERROR),
                      GsmConstStr r4 = GFP(GSM_CMS_ERROR),
#else
                      GsmConstStr r3 = NULL, GsmConstStr r4 = NULL,
#endif
                      GsmConstStr r5 = NULL) {
    /*String r1s(r1); r1s.trim();
    String r2s(r2); r2s.trim();
    String r3s(r3); r3s.trim();
    String r4s(r4); r4s.trim();
    String r5s(r5); r5s.trim();
    DBG("### ..:", r1s, ",", r2s, ",", r3s, ",", r4s, ",", r5s);*/
    data.reserve(64);
    uint8_t  index       = 0;
    uint32_t startMillis = millis();
    do {
      TINY_GSM_YIELD();
      while (stream.available() > 0) {
        TINY_GSM_YIELD();
        int8_t a = stream.read();
        if (a <= 0) continue;  // Skip 0x00 bytes, just in case
        data += static_cast<char>(a);
        if (r1 && data.endsWith(r1)) {
          index = 1;
          goto finish;
        } else if (r2 && data.endsWith(r2)) {
          index = 2;
          goto finish;
        } else if (r3 && data.endsWith(r3)) {
#if defined TINY_GSM_DEBUG
          if (r3 == GFP(GSM_CME_ERROR)) {
            streamSkipUntil('\n');  // Read out the error
          }
#endif
          index = 3;
          goto finish;
        } else if (r4 && data.endsWith(r4)) {
          index = 4;
          goto finish;
        } else if (r5 && data.endsWith(r5)) {
          index = 5;
          goto finish;
        } else if (data.endsWith(GF("+TCPRECV:"))) {//+TCPRECV(S): 1,10,1234567899
          int8_t mux = streamGetIntBefore('\n');
          DBG("### URC RECV:", mux);
          if (mux >= 0 && mux < TINY_GSM_MUX_COUNT && sockets[mux]) {
            // We have no way of knowing how much data actually came in, so
            // we set the value to 1500, the maximum possible size.
            sockets[mux]->sock_available = 1500;
          }
          data = "";
        }else if (data.endsWith(GF("+TCPCLOSE:"))) {
          int8_t mux = streamGetIntBefore(',');
          if (mux >= 0 && mux < TINY_GSM_MUX_COUNT && sockets[mux]) {
            sockets[mux]->sock_connected = false;
          }
          data = "";
          DBG("### Closed: ", mux);
        }
      }
    } while (millis() - startMillis < timeout_ms);
  finish:
    if (!index) {
      data.trim();
      if (data.length()) { DBG("### Unhandled:", data); }
      data = "";
    }
    // data.replace(GSM_NL, "/");
    // DBG('<', index, '>', data);
    return index;
  }

  int8_t waitResponse(uint32_t timeout_ms, GsmConstStr r1 = GFP(GSM_OK),
                      GsmConstStr r2 = GFP(GSM_ERROR),
#if defined TINY_GSM_DEBUG
                      GsmConstStr r3 = GFP(GSM_CME_ERROR),
                      GsmConstStr r4 = GFP(GSM_CMS_ERROR),
#else
                      GsmConstStr r3 = NULL, GsmConstStr r4 = NULL,
#endif
                      GsmConstStr r5 = NULL) {
    String data;
    return waitResponse(timeout_ms, data, r1, r2, r3, r4, r5);
  }

  int8_t waitResponse(GsmConstStr r1 = GFP(GSM_OK),
                      GsmConstStr r2 = GFP(GSM_ERROR),
#if defined TINY_GSM_DEBUG
                      GsmConstStr r3 = GFP(GSM_CME_ERROR),
                      GsmConstStr r4 = GFP(GSM_CMS_ERROR),
#else
                      GsmConstStr r3 = NULL, GsmConstStr r4 = NULL,
#endif
                      GsmConstStr r5 = NULL) {
    return waitResponse(1000, r1, r2, r3, r4, r5);
  }

 public:
  Stream& stream;

 protected:
  GsmClientN716* sockets[TINY_GSM_MUX_COUNT];
  const char*    gsmNL = GSM_NL;
};

#endif  // SRC_TINYGSMCLIENTN716_H_
