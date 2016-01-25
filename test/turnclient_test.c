
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "turnclient.h"
#include "turn_intern.h"
#include "sockaddr_util.h"
#include "test_utils.h"

#define  MAX_INSTANCES  5
#define  TEST_THREAD_CTX 1



/* static TurnCallBackData_T TurnCbData[MAX_INSTANCES]; */

/* static int            TestNo; */
/* static uint32_t StunDefaultTimeoutList[STUNCLIENT_MAX_RETRANSMITS */
/* ] = {100, 0}; */
static StunMsgId               LastTransId;
static struct sockaddr_storage LastAddress;
static bool                    runningAsIPv6;

static const uint8_t StunCookie[] = STUN_MAGIC_COOKIE_ARRAY;

TurnResult_T turnResult;

struct sockaddr_storage turnServerAddr;
TURN_INSTANCE_DATA*     pInst;


static void
TurnStatusCallBack(void*               ctx,
                   TurnCallBackData_T* retData)
{
  (void) ctx;
  turnResult = retData->turnResult;
  printf("Got TURN status callback (Result (%i)\n", retData->turnResult);

}


static void
SendRawStun(const uint8_t*         buf,
            size_t                 len,
            const struct sockaddr* addr,
            void*                  ctx)
{
  (void) ctx;
  (void) len;
  char addr_str[SOCKADDR_MAX_STRLEN];
  /* find the transaction id  so we can use this in the simulated resp */


  memcpy(&LastTransId, &buf[8], STUN_MSG_ID_SIZE);

  sockaddr_copy( (struct sockaddr*)&LastAddress, addr );

  sockaddr_toString(addr, addr_str, SOCKADDR_MAX_STRLEN, true);

  printf("TurnClienttest sendto: '%s'\n", addr_str);

}

static int
StartAllocateTransaction()
{
  runningAsIPv6 = false;
  sockaddr_initFromString( (struct sockaddr*)&turnServerAddr,
                           "158.38.48.10:3478" );
  /* kick off turn */
  return TurnClient_StartAllocateTransaction(&pInst,
                                             50,
                                             NULL,
                                             "test",
                                             NULL,
                                             (struct sockaddr*)&turnServerAddr,
                                             "pem",
                                             "pem",
                                             0,
                                             SendRawStun,
                                             TurnStatusCallBack,
                                             false,
                                             0);

}

static int
StartAllocateTransaction_IPv6()
{
  struct sockaddr_storage addr;
  sockaddr_initFromString( (struct sockaddr*)&addr,"158.38.48.10:3478" );
  runningAsIPv6 = true;
  /* kick off turn */
  return TurnClient_StartAllocateTransaction(&pInst,
                                             50,
                                             NULL,
                                             "test",
                                             NULL,
                                             (struct sockaddr*)&addr,
                                             "pem",
                                             "pem",
                                             AF_INET6,
                                             SendRawStun,
                                             TurnStatusCallBack,
                                             false,
                                             0);

}


static int
StartSSODAAllocateTransaction()
{
  sockaddr_initFromString( (struct sockaddr*)&turnServerAddr,
                           "158.38.v8.10:3478" );

  /* kick off turn */
  return TurnClient_StartAllocateTransaction(&pInst,
                                             50,
                                             NULL,
                                             "test",
                                             NULL,
                                             (struct sockaddr*)&turnServerAddr,
                                             "pem",
                                             "pem",
                                             AF_INET + AF_INET6,
                                             SendRawStun,               /* send
                                                                         * func
                                                                         **/
                                             TurnStatusCallBack,
                                             false,
                                             0);

}

static void
SimAllocResp(int  ctx,
             bool relay,
             bool xorMappedAddr,
             bool lifetime,
             bool IPv6)
{
  (void) ctx;
  StunMessage m;
  memset( &m, 0, sizeof(m) );
  memcpy( &m.msgHdr.id,     &LastTransId, STUN_MSG_ID_SIZE);
  memcpy( &m.msgHdr.cookie, StunCookie,   sizeof(m.msgHdr.cookie) );
  m.msgHdr.msgType = STUN_MSG_AllocateResponseMsg;

  /* relay */
  if (relay)
  {
    if (IPv6)
    {
      uint8_t addr[16] =
      {0x20, 0x1, 0x4, 0x70, 0xdc, 0x88, 0x0, 0x2, 0x2, 0x26, 0x18, 0xff, 0xfe,
       0x92, 0x6d, 0x53};
      m.hasXorRelayAddressIPv6 = true;
      stunlib_setIP6Address(&m.xorRelayAddressIPv6, addr, 0x4200);


    }
    else
    {
      m.hasXorRelayAddressIPv4           = true;
      m.xorRelayAddressIPv4.familyType   = STUN_ADDR_IPv4Family;
      m.xorRelayAddressIPv4.addr.v4.addr = 3251135384UL;      /*
                                                               *
                                                               *"193.200.99.152"
                                                               **/
      m.xorRelayAddressIPv4.addr.v4.port = 42000;
    }
  }

  /* XOR mapped addr*/
  if (xorMappedAddr)
  {
    if (IPv6)
    {
      uint8_t addr[16] =
      {0x20, 0x1, 0x4, 0x70, 0xdc, 0x88, 0x1, 0x22, 0x21, 0x26, 0x18, 0xff,
       0xfe, 0x92, 0x6d, 0x53};
      m.hasXorMappedAddress = true;
      stunlib_setIP6Address(&m.xorMappedAddress, addr, 0x4200);

    }
    else
    {

      m.hasXorMappedAddress           = true;
      m.xorMappedAddress.familyType   = STUN_ADDR_IPv4Family;
      m.xorMappedAddress.addr.v4.addr = 1009527574UL;      /* "60.44.43.22"); */
      m.xorMappedAddress.addr.v4.port = 43000;
    }
  }

  /* lifetime */
  if (lifetime)
  {
    m.hasLifetime    = true;
    m.lifetime.value = 60;
  }

  TurnClient_HandleIncResp(pInst, &m, NULL);

}

static void
SimSSODAAllocResp(int  ctx,
                  bool relay,
                  bool xorMappedAddr,
                  bool lifetime)
{
  (void) ctx;
  StunMessage m;
  memset( &m, 0, sizeof(m) );
  memcpy( &m.msgHdr.id,     &LastTransId, STUN_MSG_ID_SIZE);
  memcpy( &m.msgHdr.cookie, StunCookie,   sizeof(m.msgHdr.cookie) );
  m.msgHdr.msgType = STUN_MSG_AllocateResponseMsg;

  /* relay */
  if (relay)
  {
    uint8_t addr[16] =
    {0x20, 0x1, 0x4, 0x70, 0xdc, 0x88, 0x0, 0x2, 0x2, 0x26, 0x18, 0xff, 0xfe,
     0x92, 0x6d, 0x53};
    m.hasXorRelayAddressIPv6 = true;
    stunlib_setIP6Address(&m.xorRelayAddressIPv6, addr, 0x4200);



    m.hasXorRelayAddressIPv4           = true;
    m.xorRelayAddressIPv4.familyType   = STUN_ADDR_IPv4Family;
    m.xorRelayAddressIPv4.addr.v4.addr = 3251135384UL;    /* "193.200.99.152" */
    m.xorRelayAddressIPv4.addr.v4.port = 42000;

  }

  /* XOR mapped addr*/
  if (xorMappedAddr)
  {

    m.hasXorMappedAddress           = true;
    m.xorMappedAddress.familyType   = STUN_ADDR_IPv4Family;
    m.xorMappedAddress.addr.v4.addr = 1009527574UL;    /* "60.44.43.22"); */
    m.xorMappedAddress.addr.v4.port = 43000;

  }

  /* lifetime */
  if (lifetime)
  {
    m.hasLifetime    = true;
    m.lifetime.value = 60;
  }

  TurnClient_HandleIncResp(pInst, &m, NULL);

}

static void
Sim_ChanBindOrPermissionResp(int      ctx,
                             uint16_t msgType,
                             uint32_t errClass,
                             uint32_t errNumber)
{
  (void) ctx;
  StunMessage m;
  memset( &m, 0, sizeof(m) );
  memcpy( &m.msgHdr.id,     &LastTransId, STUN_MSG_ID_SIZE);
  memcpy( &m.msgHdr.cookie, StunCookie,   sizeof(m.msgHdr.cookie) );
  m.msgHdr.msgType = msgType;

  if ( (msgType == STUN_MSG_ChannelBindErrorResponseMsg)
       || (msgType == STUN_MSG_CreatePermissionErrorResponseMsg) )
  {
    m.hasErrorCode         = true;
    m.errorCode.errorClass = errClass;
    m.errorCode.number     = errNumber;
  }
  TurnClient_HandleIncResp(pInst, &m, NULL);
}


static void
SimInitialAllocRespErr(int      ctx,
                       bool     hasErrCode,
                       uint32_t errClass,
                       uint32_t errNumber,
                       bool     hasRealm,
                       bool     hasNonce,
                       bool     hasAltServer)
{
  (void) ctx;
  StunMessage m;

  memset( &m, 0, sizeof(m) );
  memcpy( &m.msgHdr.id,     &LastTransId, STUN_MSG_ID_SIZE);
  memcpy( &m.msgHdr.cookie, StunCookie,   sizeof(m.msgHdr.cookie) );
  m.msgHdr.msgType = STUN_MSG_AllocateErrorResponseMsg;

  /* error code */
  if (hasErrCode)
  {
    m.hasErrorCode         = hasErrCode;
    m.errorCode.errorClass = errClass;
    m.errorCode.number     = errNumber;
  }

  /* realm */
  if (hasRealm)
  {
    m.hasRealm = hasRealm;
    strncpy( m.realm.value, "united.no", strlen("united.no") );
    m.realm.sizeValue = strlen(m.realm.value);
  }

  /* nonce */
  if (hasNonce)
  {
    m.hasNonce = hasNonce;
    strncpy( m.nonce.value, "mydaftnonce", strlen("mydaftnonce") );
    m.nonce.sizeValue = strlen(m.nonce.value);
  }


  /* alternate server */
  if (hasAltServer)
  {
    m.hasAlternateServer = true;
    if (runningAsIPv6)
    {
      uint8_t addr[16] =
      {0x20, 0x1, 0x4, 0x70, 0xdc, 0x88, 0x0, 0x2, 0x2, 0x26, 0x18, 0xff, 0xfe,
       0x92, 0x6d, 0x53};
      stunlib_setIP6Address(&m.alternateServer, addr, 0x4200);


    }
    else
    {

      m.alternateServer.familyType   = STUN_ADDR_IPv4Family;
      m.alternateServer.addr.v4.addr = 0x12345678;
      m.alternateServer.addr.v4.port = 3478;
    }
  }


  TurnClient_HandleIncResp(pInst, &m, NULL);
}



static void
Sim_RefreshResp(int ctx)
{
  (void) ctx;
  TurnClientSimulateSig(pInst, TURN_SIGNAL_RefreshResp);
}


/* allocation refresh error */
static void
Sim_RefreshError(int      ctx,
                 uint32_t errClass,
                 uint32_t errNumber,
                 bool     hasRealm,
                 bool     hasNonce)
{
  (void) ctx;
  StunMessage m;
  memset( &m, 0, sizeof(m) );
  memcpy( &m.msgHdr.id,     &LastTransId, STUN_MSG_ID_SIZE);
  memcpy( &m.msgHdr.cookie, StunCookie,   sizeof(m.msgHdr.cookie) );
  m.msgHdr.msgType       = STUN_MSG_RefreshErrorResponseMsg;
  m.hasErrorCode         = true;
  m.errorCode.errorClass = errClass;
  m.errorCode.number     = errNumber;

  /* realm */
  if (hasRealm)
  {
    m.hasRealm = hasRealm;
    strncpy( m.realm.value, "united.no", strlen("united.no") );
    m.realm.sizeValue = strlen(m.realm.value);
  }

  /* nonce */
  if (hasNonce)
  {
    m.hasNonce = hasNonce;
    strncpy( m.nonce.value, "mydaftnonce", strlen("mydaftnonce") );
    m.nonce.sizeValue = strlen(m.nonce.value);
  }

  TurnClient_HandleIncResp(pInst, &m, NULL);
}


static int
GotoAllocatedState(int appCtx)
{
  int ctx = StartAllocateTransaction();
  TurnClient_HandleTick(pInst);
  SimInitialAllocRespErr(appCtx, true, 4, 1, true, true, false);   /* 401, has
                                                                    * realm has
                                                                    * nonce */
  TurnClient_HandleTick(pInst);
  SimAllocResp(ctx, true, true, true, runningAsIPv6);
  return ctx;
}


static void
Sim_TimerRefreshAlloc(int ctx)
{
  (void) ctx;
  TurnClientSimulateSig(pInst, TURN_SIGNAL_TimerRefreshAlloc);
}

static void
Sim_TimerRefreshChannelBind(int ctx)
{
  (void) ctx;
  TurnClientSimulateSig(pInst, TURN_SIGNAL_TimerRefreshChannel);
}

static void
Sim_TimerRefreshPermission(int ctx)
{
  (void) ctx;
  TurnClientSimulateSig(pInst, TURN_SIGNAL_TimerRefreshPermission);
}

CTEST(turnclient, WaitAllocRespNotAut_Timeout)
{
  StartAllocateTransaction();

  /* 1 Tick */
  TurnClient_HandleTick(pInst);
  ASSERT_TRUE( turnResult == TurnResult_Empty);
  ASSERT_TRUE( sockaddr_alike( (struct sockaddr*)&LastAddress,
                               (struct sockaddr*)&turnServerAddr ) );
  /* 2 Tick */
  TurnClient_HandleTick(pInst);
  ASSERT_TRUE(turnResult == TurnResult_Empty);

  /* 3 Tick */
  TurnClient_HandleTick(pInst);
  ASSERT_TRUE(turnResult == TurnResult_Empty);

  /* 4 Tick */
  {
    int i;
    for (i = 0; i < 100; i++)
    {
      TurnClient_HandleTick(pInst);
    }
  }
  ASSERT_TRUE(turnResult == TurnResult_AllocFailNoAnswer);

}

CTEST(tunrclient, resultToString)
{
  ASSERT_TRUE(strcmp(TurnResultToStr(TurnResult_AllocOk),
                     "TurnResult_AllocOk") == 0);

  ASSERT_TRUE(strcmp(TurnResultToStr(TurnResult_AllocFail),
                     "TurnResult_AllocFail") == 0);

  ASSERT_TRUE(strcmp(TurnResultToStr(TurnResult_AllocFailNoAnswer),
                     "TurnResult_AllocFailNoAnswer") == 0);

  ASSERT_TRUE(strcmp(TurnResultToStr(TurnResult_AllocUnauthorised),
                     "TurnResult_AllocUnauthorised") == 0);

  ASSERT_TRUE(strcmp(TurnResultToStr(TurnResult_CreatePermissionOk),
                     "TurnResult_CreatePermissionOk") == 0);

  ASSERT_TRUE(strcmp(TurnResultToStr(TurnResult_CreatePermissionFail),
                     "TurnResult_CreatePermissionFail") == 0);

  ASSERT_TRUE(strcmp(TurnResultToStr(TurnResult_CreatePermissionNoAnswer),
                     "TurnResult_CreatePermissionNoAnswer") == 0);

  ASSERT_TRUE(strcmp(TurnResultToStr(TurnResult_CreatePermissionQuotaReached),
                     "TurnResult_CreatePermissionQuotaReached") == 0);

  ASSERT_TRUE(strcmp(TurnResultToStr(TurnResult_PermissionRefreshFail),
                     "TurnResult_PermissionRefreshFail") == 0);

  ASSERT_TRUE(strcmp(TurnResultToStr(TurnResult_ChanBindOk),
                     "TurnResult_ChanBindOk") == 0);

  ASSERT_TRUE(strcmp(TurnResultToStr(TurnResult_ChanBindFail),
                     "TurnResult_ChanBindFail") == 0);

  ASSERT_TRUE(strcmp(TurnResultToStr(TurnResult_ChanBindFailNoanswer),
                     "TurnResult_ChanBindFailNoanswer") == 0);

  ASSERT_TRUE(strcmp(TurnResultToStr(TurnResult_RefreshFail),
                     "TurnResult_RefreshFail") == 0);

  ASSERT_TRUE(strcmp(TurnResultToStr(TurnResult_RefreshFailNoAnswer),
                     "TurnResult_RefreshFailNoAnswer") == 0);

  ASSERT_TRUE(strcmp(TurnResultToStr(TurnResult_RelayReleaseComplete),
                     "TurnResult_RelayReleaseComplete") == 0);

  ASSERT_TRUE(strcmp(TurnResultToStr(TurnResult_RelayReleaseFailed),
                     "TurnResult_RelayReleaseFailed") == 0);

  ASSERT_TRUE(strcmp(TurnResultToStr(TurnResult_InternalError),
                     "TurnResult_InternalError") == 0);

  ASSERT_TRUE(strcmp(TurnResultToStr(TurnResult_MalformedRespWaitAlloc),
                     "TurnResult_MalformedRespWaitAlloc") == 0);

  ASSERT_TRUE(strcmp(TurnResultToStr(TurnResult_Empty),
                     "unknown turnresult ??") == 0);
}

CTEST(turnclient, WaitAllocRespNotAut_AllocRspOk)
{
  int ctx;
  ctx = StartAllocateTransaction();
  TurnClient_HandleTick(pInst);
  SimAllocResp(ctx, true, true, true, runningAsIPv6);
  ASSERT_TRUE(turnResult == TurnResult_AllocOk);

  TurnClient_Deallocate(pInst);
  Sim_RefreshResp(ctx);
  ASSERT_TRUE(turnResult == TurnResult_RelayReleaseComplete);

}

CTEST(turnclient, WaitAllocRespNotAutSSODA_AllocRspOk)
{
  int ctx;
  ctx = StartAllocateTransaction();
  TurnClient_HandleTick(pInst);
  SimSSODAAllocResp(ctx, true, true, true);
  ASSERT_TRUE(turnResult == TurnResult_AllocOk);

  TurnClient_Deallocate(pInst);
  Sim_RefreshResp(ctx);
  ASSERT_TRUE(turnResult == TurnResult_RelayReleaseComplete);

}

CTEST(turnclient, WaitAllocRespNotAut_AllocRspErr_AltServer)
{
  int ctx;
  ctx = StartAllocateTransaction();
  TurnClient_HandleTick(pInst);
  SimInitialAllocRespErr(ctx, true, 3, 0, false, false, true);    /* 300, alt
                                                                  * server */
  ASSERT_FALSE(turnResult == TurnResult_Empty);

  TurnClient_HandleTick(pInst);
  SimInitialAllocRespErr(ctx, true, 4, 1, true, true, false);   /* 401, has
                                                                 * realm and
                                                                 * nonce */
  ASSERT_FALSE(turnResult == TurnResult_Empty);

  TurnClient_HandleTick(pInst);
  SimAllocResp(ctx, true, true, true, runningAsIPv6);
  ASSERT_TRUE(turnResult == TurnResult_AllocOk);

  ASSERT_TRUE (TurnClient_hasBeenRedirected(pInst) );
  ASSERT_TRUE ( sockaddr_alike( (struct sockaddr*)&LastAddress,
                                TurnClient_getRedirectedServerAddr(pInst) ) );

  TurnClient_Deallocate(pInst);
  Sim_RefreshResp(ctx);
  ASSERT_TRUE(turnResult == TurnResult_RelayReleaseComplete);

}



CTEST(turnclient, WaitAllocRespNotAut_AllocRsp_Malf1)
{
  int ctx;
  ctx = StartAllocateTransaction();
  TurnClient_HandleTick(pInst);
  SimAllocResp(ctx, false, true, true, runningAsIPv6);
  ASSERT_TRUE(turnResult == TurnResult_MalformedRespWaitAlloc);
  TurnClient_Deallocate(pInst);

}



CTEST(turnclient, WaitAllocRespNotAut_AllocRsp_Malf2)
{
  int ctx;
  ctx = StartAllocateTransaction();
  TurnClient_HandleTick(pInst);
  SimAllocResp(ctx, true, false, true, runningAsIPv6);
  ASSERT_TRUE(turnResult == TurnResult_MalformedRespWaitAlloc);
  TurnClient_Deallocate(pInst);

}



CTEST(turnclient, WaitAllocRespNotAut_AllocRsp_Malf3)
{
  int ctx;
  ctx = StartAllocateTransaction();
  TurnClient_HandleTick(pInst);
  SimAllocResp(ctx, true, true, false, runningAsIPv6);
  ASSERT_TRUE(turnResult == TurnResult_MalformedRespWaitAlloc);
  TurnClient_Deallocate(pInst);

}



CTEST(turnclient, WaitAllocRespNotAut_AllocRspErr_Ok)
{
  int ctx;
  ctx = StartAllocateTransaction();
  TurnClient_HandleTick(pInst);
  SimInitialAllocRespErr(ctx, true, 4, 1, true, true, false);   /* 401, has
                                                                 * realm and
                                                                 * nonce */
  ASSERT_FALSE(turnResult == TurnResult_Empty);

  TurnClient_HandleTick(pInst);
  SimAllocResp(ctx, true, true, true, runningAsIPv6);
  ASSERT_TRUE(turnResult == TurnResult_AllocOk);
  ASSERT_FALSE (TurnClient_hasBeenRedirected(pInst) );
  ASSERT_FALSE ( sockaddr_alike( (struct sockaddr*)&LastAddress,
                                TurnClient_getRedirectedServerAddr(pInst) ) );
  TurnClient_Deallocate(pInst);
  Sim_RefreshResp(ctx);
  ASSERT_TRUE(turnResult == TurnResult_RelayReleaseComplete);
}


CTEST(turnclient, WaitAllocRespNotAut_AllocRspErr_ErrNot401)
{
  int ctx;
  ctx = StartAllocateTransaction();
  TurnClient_HandleTick(pInst);
  SimInitialAllocRespErr(ctx, true, 4, 4, false, false, false);   /* 404, no
                                                                  * realm, no
                                                                  * nonce */
  TurnClient_HandleTick(pInst);
  ASSERT_TRUE(turnResult == TurnResult_MalformedRespWaitAlloc);
  TurnClient_Deallocate(pInst);

}



CTEST(turnclient, WaitAllocRespNotAut_AllocRspErr_Err_malf1)
{
  int ctx;
  ctx = StartAllocateTransaction();
  TurnClient_HandleTick(pInst);
  SimInitialAllocRespErr(ctx, true, 4, 1, false, true, false);   /* 401, no
                                                                  * realm, nonce
                                                                  **/
  TurnClient_HandleTick(pInst);
  ASSERT_TRUE(turnResult == TurnResult_MalformedRespWaitAlloc);
  TurnClient_Deallocate(pInst);
}


CTEST(turnclient, WaitAllocRespNotAut_AllocRspErr_Err_malf2)
{
  int ctx;
  ctx = StartAllocateTransaction();
  TurnClient_HandleTick(pInst);
  SimInitialAllocRespErr(ctx, true, 4, 1, true, false, false);   /* 401, realm,
                                                                  * no nonce */
  TurnClient_HandleTick(pInst);
  ASSERT_TRUE(turnResult == TurnResult_MalformedRespWaitAlloc);
  TurnClient_Deallocate(pInst);

}



CTEST(turnclient, WaitAllocRespNotAut_AllocRspErr_Err_malf3)
{
  int ctx;
  ctx = StartAllocateTransaction();
  TurnClient_HandleTick(pInst);
  SimInitialAllocRespErr(ctx, true, 3, 0, false, false, false);    /* 300,
                                                                    * missing
                                                                    * alt
                                                                    * server */
  TurnClient_HandleTick(pInst);
  ASSERT_TRUE(turnResult == TurnResult_MalformedRespWaitAlloc);
  TurnClient_Deallocate(pInst);

}


CTEST(turnclient, WaitAllocResp_AllocRespOk)
{
  int ctx;
  ctx = StartAllocateTransaction();
  TurnClient_HandleTick(pInst);
  SimInitialAllocRespErr(ctx, true, 4, 1, true, true, false);   /* 401, has
                                                                 * realm and
                                                                 * nonce */
  TurnClient_HandleTick(pInst);
  SimAllocResp(ctx, true, true, true, runningAsIPv6);
  ASSERT_TRUE(turnResult == TurnResult_AllocOk);
  TurnClient_Deallocate(pInst);
  Sim_RefreshResp(ctx);
  ASSERT_TRUE(turnResult == TurnResult_RelayReleaseComplete);
  TurnClient_free(pInst);

}

CTEST(turnclient, WaitAllocResp_AllocRespOk_IPv6)
{
  int ctx;
  ctx = StartAllocateTransaction_IPv6();
  TurnClient_HandleTick(pInst);
  SimInitialAllocRespErr(ctx, true, 4, 1, true, true, false);   /* 401, has
                                                                 * realm and
                                                                 * nonce */
  TurnClient_HandleTick(pInst);
  SimAllocResp(ctx, true, true, true, runningAsIPv6);
  ASSERT_TRUE(turnResult == TurnResult_AllocOk);
  TurnClient_Deallocate(pInst);
  Sim_RefreshResp(ctx);
  ASSERT_TRUE(turnResult == TurnResult_RelayReleaseComplete);
  TurnClient_free(pInst);

}


CTEST(turnclient, WaitAllocResp_SSODA_AllocRespOk)
{
  int ctx;
  ctx = StartSSODAAllocateTransaction();
  TurnClient_HandleTick(pInst);
  SimInitialAllocRespErr(ctx, true, 4, 1, true, true, false);   /* 401, has
                                                                 * realm and
                                                                 * nonce */
  TurnClient_HandleTick(pInst);
  SimAllocResp(ctx, true, true, true, runningAsIPv6);
  ASSERT_TRUE(turnResult == TurnResult_AllocOk);
  TurnClient_Deallocate(pInst);
  Sim_RefreshResp(ctx);
  ASSERT_TRUE(turnResult == TurnResult_RelayReleaseComplete);

}



CTEST(turnclient, WaitAllocResp_AllocRespErr)
{
  int ctx;
  ctx = StartAllocateTransaction();
  TurnClient_HandleTick(pInst);
  SimInitialAllocRespErr(ctx, true, 4, 1, true, true, false);      /* 401, has
                                                                    * realm and
                                                                    * nonce */
  ASSERT_FALSE(turnResult == TurnResult_Empty);


  TurnClient_HandleTick(pInst);
  SimInitialAllocRespErr(ctx, true, 4, 4, false, false, false);    /* 404, no
                                                                    * realm and
                                                                    * no nonce
                                                                    **/


  ASSERT_TRUE(turnResult == TurnResult_AllocUnauthorised);

  TurnClient_Deallocate(pInst);

}


CTEST(turnclient, WaitAllocResp_Retry)
{
  int ctx, i;
  ctx = StartAllocateTransaction();
  TurnClient_HandleTick(pInst);
  SimInitialAllocRespErr(ctx, true, 4, 1, true, true, false);      /* 401, has
                                                                    * realm and
                                                                    * nonce */
  ASSERT_FALSE(turnResult == TurnResult_Empty);
  for (i = 0; i < 100; i++)
  {
    TurnClient_HandleTick(pInst);
  }

  ASSERT_TRUE(turnResult == TurnResult_AllocFailNoAnswer);

  TurnClient_Deallocate(pInst);
}


CTEST(turnclient, Allocated_RefreshOk)
{
  int ctx, i;

  ctx = GotoAllocatedState(9);

  for (i = 0; i < 2; i++)
  {
    Sim_TimerRefreshAlloc(ctx);
    TurnClient_HandleTick(pInst);
    Sim_RefreshResp(ctx);
  }
  ASSERT_TRUE(turnResult == TurnResult_AllocOk);

  TurnClient_Deallocate(pInst);
  Sim_RefreshResp(ctx);
  ASSERT_TRUE(turnResult == TurnResult_RelayReleaseComplete);


}


CTEST(turnclient, Allocated_RefreshError)
{
  int ctx;
  ctx = GotoAllocatedState(4);

  Sim_TimerRefreshAlloc(ctx);
  TurnClient_HandleTick(pInst);
  Sim_RefreshResp(ctx);
  ASSERT_TRUE(turnResult == TurnResult_AllocOk);

  Sim_TimerRefreshAlloc(ctx);
  TurnClient_HandleTick(pInst);
  Sim_RefreshError(ctx, 4, 1, false, false);
  ASSERT_TRUE(turnResult == TurnResult_RefreshFail);

}



CTEST(turnclient, Allocated_StaleNonce)
{
  int ctx;
  ctx = GotoAllocatedState(4);

  Sim_TimerRefreshAlloc(ctx);
  TurnClient_HandleTick(pInst);
  Sim_RefreshResp(ctx);

  Sim_TimerRefreshAlloc(ctx);
  TurnClient_HandleTick(pInst);
  Sim_RefreshError(ctx, 4, 38, true, true);   /* stale nonce */

  TurnClient_HandleTick(pInst);
  Sim_RefreshResp(ctx);
  ASSERT_TRUE(turnResult == TurnResult_AllocOk);

  TurnClient_Deallocate(pInst);
  Sim_RefreshResp(ctx);
  ASSERT_TRUE(turnResult == TurnResult_RelayReleaseComplete);

}


CTEST(turnclient, Allocated_ChanBindReqOk)
{
  struct sockaddr_storage peerIp;
  int                     ctx;
  sockaddr_initFromString( (struct sockaddr*)&peerIp,"192.168.5.22:1234" );

  ctx = GotoAllocatedState(12);
  TurnClient_StartChannelBindReq(pInst, 0x4001, (struct sockaddr*)&peerIp);
  TurnClient_HandleTick(pInst);
  Sim_ChanBindOrPermissionResp(ctx, STUN_MSG_ChannelBindResponseMsg, 0, 0);
  TurnClient_HandleTick(pInst);
  ASSERT_TRUE(turnResult == TurnResult_ChanBindOk);

  TurnClient_Deallocate(pInst);
  Sim_RefreshResp(ctx);
  ASSERT_TRUE(turnResult == TurnResult_RelayReleaseComplete);

}


CTEST(turnclient, Allocated_ChanBindRefresh)
{
  struct sockaddr_storage peerIp;
  int                     ctx;
  sockaddr_initFromString( (struct sockaddr*)&peerIp,"192.168.5.22:1234" );

  ctx = GotoAllocatedState(12);
  TurnClient_StartChannelBindReq(pInst, 0x4001, (struct sockaddr*)&peerIp);
  TurnClient_HandleTick(pInst);
  Sim_ChanBindOrPermissionResp(ctx, STUN_MSG_ChannelBindResponseMsg, 0, 0);
  TurnClient_HandleTick(pInst);
  ASSERT_TRUE(turnResult == TurnResult_ChanBindOk);

  /* verfiy callback is not called again */
  turnResult = TurnResult_Empty;
  Sim_TimerRefreshChannelBind(ctx);
  Sim_ChanBindOrPermissionResp(ctx, STUN_MSG_ChannelBindResponseMsg, 0, 0);
  ASSERT_TRUE(turnResult == TurnResult_Empty);

  TurnClient_Deallocate(pInst);
  Sim_RefreshResp(ctx);
  ASSERT_TRUE(turnResult == TurnResult_RelayReleaseComplete);
}



CTEST(turnclient, Allocated_ChanBindErr)
{
  struct sockaddr_storage peerIp;
  int                     ctx;
  sockaddr_initFromString( (struct sockaddr*)&peerIp,"192.168.5.22:1234" );

  ctx = GotoAllocatedState(12);
  TurnClient_StartChannelBindReq(pInst, 0x4001, (struct sockaddr*)&peerIp);
  TurnClient_HandleTick(pInst);
  Sim_ChanBindOrPermissionResp(ctx, STUN_MSG_ChannelBindErrorResponseMsg, 4, 4);
  TurnClient_HandleTick(pInst);
  ASSERT_TRUE(turnResult == TurnResult_ChanBindFail);

  TurnClient_Deallocate(pInst);
  Sim_RefreshResp(ctx);
  ASSERT_TRUE(turnResult == TurnResult_RelayReleaseComplete);
}


CTEST(turnclient, Allocated_CreatePermissionReqOk)
{
  struct sockaddr_storage  peerIp[6];
  struct sockaddr_storage* p_peerIp[6];
  int                      ctx;
  uint32_t                 i;

  for (i = 0; i < sizeof(peerIp) / sizeof(peerIp[0]); i++)
  {
    sockaddr_initFromString( (struct sockaddr*)&peerIp[i],"192.168.5.22:1234" );
    p_peerIp[i] = &peerIp[i];
  }

  ctx = GotoAllocatedState(12);
  TurnClient_StartCreatePermissionReq(pInst,
                                      sizeof(peerIp) / sizeof(peerIp[0]),
                                      (const struct sockaddr**)p_peerIp);
  TurnClient_HandleTick(pInst);
  Sim_ChanBindOrPermissionResp(ctx, STUN_MSG_CreatePermissionResponseMsg, 0, 0);
  TurnClient_HandleTick(pInst);
  ASSERT_TRUE(turnResult == TurnResult_CreatePermissionOk);
  TurnClient_Deallocate(pInst);
  Sim_RefreshResp(ctx);
  ASSERT_TRUE(turnResult == TurnResult_RelayReleaseComplete);
}


CTEST(turnclient, Allocated_CreatePermissionRefresh)
{
  struct sockaddr_storage  peerIp[6];
  struct sockaddr_storage* p_peerIp[6];
  int                      ctx;
  uint32_t                 i;

  for (i = 0; i < sizeof(peerIp) / sizeof(peerIp[0]); i++)
  {
    sockaddr_initFromString( (struct sockaddr*)&peerIp[i],"192.168.5.22:1234" );
    p_peerIp[i] = &peerIp[i];
  }

  ctx = GotoAllocatedState(12);
  TurnClient_StartCreatePermissionReq(pInst,
                                      sizeof(peerIp) / sizeof(peerIp[0]),
                                      (const struct sockaddr**)p_peerIp);
  TurnClient_HandleTick(pInst);
  Sim_ChanBindOrPermissionResp(ctx, STUN_MSG_CreatePermissionResponseMsg, 0, 0);
  TurnClient_HandleTick(pInst);
  ASSERT_TRUE(turnResult == TurnResult_CreatePermissionOk);

  /* verfiy callback is not called again */
  turnResult = TurnResult_Empty;
  Sim_TimerRefreshPermission(ctx);
  Sim_ChanBindOrPermissionResp(ctx, STUN_MSG_CreatePermissionResponseMsg, 0, 0);
  ASSERT_TRUE(turnResult == TurnResult_Empty);

  TurnClient_Deallocate(pInst);
  Sim_RefreshResp(ctx);
  ASSERT_TRUE(turnResult == TurnResult_RelayReleaseComplete);
}





CTEST(turnclient, Allocated_CreatePermissionErr)
{
  struct sockaddr_storage  peerIp[3];
  struct sockaddr_storage* p_peerIp[3];
  int                      ctx;
  uint32_t                 i;

  for (i = 0; i < sizeof(peerIp) / sizeof(peerIp[0]); i++)
  {
    sockaddr_initFromString( (struct sockaddr*)&peerIp[i],"192.168.5.22:1234" );
    p_peerIp[i] = &peerIp[i];
  }

  ctx = GotoAllocatedState(12);
  TurnClient_StartCreatePermissionReq(pInst,
                                      sizeof(peerIp) / sizeof(peerIp[0]),
                                      (const struct sockaddr**)p_peerIp);
  TurnClient_HandleTick(pInst);
  Sim_ChanBindOrPermissionResp(ctx,
                               STUN_MSG_CreatePermissionErrorResponseMsg,
                               4,
                               4);
  TurnClient_HandleTick(pInst);
  ASSERT_TRUE(turnResult == TurnResult_PermissionRefreshFail);
  TurnClient_Deallocate(pInst);
  Sim_RefreshResp(ctx);
  ASSERT_TRUE(turnResult == TurnResult_RelayReleaseComplete);
}


CTEST(turnclient, Allocated_CreatePermissionReqAndChannelBind)
{
  struct sockaddr_storage  peerIp[6];
  struct sockaddr_storage* p_peerIp[6];
  int                      ctx;
  uint32_t                 i;

  for (i = 0; i < sizeof(peerIp) / sizeof(peerIp[0]); i++)
  {
    sockaddr_initFromString( (struct sockaddr*)&peerIp[i],"192.168.5.22:1234" );
    p_peerIp[i] = &peerIp[i];
  }

  ctx = GotoAllocatedState(12);
  TurnClient_StartCreatePermissionReq(pInst,
                                      sizeof(peerIp) / sizeof(peerIp[0]),
                                      (const struct sockaddr**)p_peerIp);
  TurnClient_StartChannelBindReq(pInst, 0x4001, (struct sockaddr*)&peerIp[0]);

  TurnClient_HandleTick(pInst);
  Sim_ChanBindOrPermissionResp(ctx, STUN_MSG_CreatePermissionResponseMsg, 0, 0);
  TurnClient_HandleTick(pInst);
  ASSERT_TRUE(turnResult == TurnResult_CreatePermissionOk);

  Sim_ChanBindOrPermissionResp(ctx, STUN_MSG_ChannelBindResponseMsg, 0, 0);
  TurnClient_HandleTick(pInst);
  ASSERT_TRUE(turnResult == TurnResult_ChanBindOk);

  TurnClient_Deallocate(pInst);
  Sim_RefreshResp(ctx);
  ASSERT_TRUE(turnResult == TurnResult_RelayReleaseComplete);
}


CTEST(turnclient, Allocated_CreatePermissionErrorAndChannelBind)
{
  struct sockaddr_storage  peerIp[6];
  struct sockaddr_storage* p_peerIp[6];
  int                      ctx;
  uint32_t                 i;

  for (i = 0; i < sizeof(peerIp) / sizeof(peerIp[0]); i++)
  {
    sockaddr_initFromString( (struct sockaddr*)&peerIp[i],"192.168.5.22:1234" );
    p_peerIp[i] = &peerIp[i];
  }

  ctx = GotoAllocatedState(12);
  TurnClient_StartCreatePermissionReq(pInst,
                                      sizeof(peerIp) / sizeof(peerIp[0]),
                                      (const struct sockaddr**)p_peerIp);
  TurnClient_StartChannelBindReq(pInst, 0x4001, (struct sockaddr*)&peerIp[0]);

  TurnClient_HandleTick(pInst);
  Sim_ChanBindOrPermissionResp(ctx,
                               STUN_MSG_CreatePermissionErrorResponseMsg,
                               4,
                               4);
  TurnClient_HandleTick(pInst);
  ASSERT_TRUE(turnResult == TurnResult_PermissionRefreshFail);

  Sim_ChanBindOrPermissionResp(ctx, STUN_MSG_ChannelBindResponseMsg, 0, 0);
  TurnClient_HandleTick(pInst);
  ASSERT_TRUE(turnResult == TurnResult_ChanBindOk);

  TurnClient_Deallocate(pInst);
  Sim_RefreshResp(ctx);
  ASSERT_TRUE(turnResult == TurnResult_RelayReleaseComplete);
}


CTEST(turnclient, GetMessageName)
{
  ASSERT_TRUE( 0 ==
               strcmp(stunlib_getMessageName(STUN_MSG_BindRequestMsg),
                      "BindRequest") );
  ASSERT_TRUE( 0 ==
               strcmp(stunlib_getMessageName(STUN_MSG_BindResponseMsg),
                      "BindResponse") );
  ASSERT_TRUE( 0 ==
               strcmp(stunlib_getMessageName(STUN_MSG_BindIndicationMsg),
                      "BindInd") );
  ASSERT_TRUE( 0 ==
               strcmp(stunlib_getMessageName(STUN_MSG_BindErrorResponseMsg),
                      "BindErrorResponse") );
  ASSERT_TRUE( 0 ==
               strcmp(stunlib_getMessageName(STUN_MSG_AllocateRequestMsg),
                      "AllocateRequest") );
  ASSERT_TRUE( 0 ==
               strcmp(stunlib_getMessageName(
                        STUN_MSG_AllocateResponseMsg), "AllocateResponse") );
  ASSERT_TRUE( 0 ==
               strcmp(stunlib_getMessageName(STUN_MSG_AllocateErrorResponseMsg),
                      "AllocateErrorResponse") );
  ASSERT_TRUE( 0 ==
               strcmp(stunlib_getMessageName(STUN_MSG_CreatePermissionRequestMsg),
                      "CreatePermissionReq") );
  ASSERT_TRUE( 0 ==
               strcmp(stunlib_getMessageName(
                        STUN_MSG_CreatePermissionResponseMsg),
                      "CreatePermissionResp") );
  ASSERT_TRUE( 0 ==
               strcmp(stunlib_getMessageName(
                        STUN_MSG_CreatePermissionErrorResponseMsg),
                      "CreatePermissionError") );
  ASSERT_TRUE( 0 ==
               strcmp(stunlib_getMessageName(STUN_MSG_ChannelBindRequestMsg),
                      "ChannelBindRequest") );
  ASSERT_TRUE( 0 ==
               strcmp(stunlib_getMessageName(STUN_MSG_ChannelBindResponseMsg),
                      "ChannelBindResponse") );
  ASSERT_TRUE( 0 ==
               strcmp(stunlib_getMessageName(
                        STUN_MSG_ChannelBindErrorResponseMsg),
                      "ChannelBindErrorResponse") );
  ASSERT_TRUE( 0 ==
               strcmp(stunlib_getMessageName(STUN_MSG_RefreshRequestMsg),
                      "RefreshRequest") );
  ASSERT_TRUE( 0 ==
               strcmp(stunlib_getMessageName(
                        STUN_MSG_RefreshResponseMsg),   "RefreshResponse") );
  ASSERT_TRUE( 0 ==
               strcmp(stunlib_getMessageName(STUN_MSG_RefreshErrorResponseMsg),
                      "RefreshErrorResponse") );
  ASSERT_TRUE( 0 ==
               strcmp(stunlib_getMessageName(STUN_MSG_DataIndicationMsg),
                      "DataIndication") );
  ASSERT_TRUE( 0 ==
               strcmp(stunlib_getMessageName(
                        STUN_MSG_SendIndicationMsg),   "STUN_MSG_SendInd") );



  ASSERT_TRUE( 0 == strcmp(stunlib_getMessageName(123),   "???") );

}



CTEST(turnclient, SuccessResp)
{
  StunMessage msg;

  msg.msgHdr.msgType = STUN_MSG_BindResponseMsg;
  ASSERT_TRUE( stunlib_isSuccessResponse(&msg) );

}


CTEST(turnclient, ErrorResp)
{
  StunMessage msg;

  msg.msgHdr.msgType = STUN_MSG_BindErrorResponseMsg;
  ASSERT_TRUE( stunlib_isErrorResponse(&msg) );

}


CTEST(turnclient, Resp)
{
  StunMessage msg;

  msg.msgHdr.msgType = STUN_MSG_BindErrorResponseMsg;
  ASSERT_TRUE( stunlib_isResponse(&msg) );

}


CTEST(turnclient, Ind)
{
  StunMessage msg;

  msg.msgHdr.msgType = STUN_MSG_SendIndicationMsg;
  ASSERT_TRUE( stunlib_isIndication(&msg) );

}



CTEST(turnclient, Req)
{
  StunMessage msg;

  msg.msgHdr.msgType = STUN_MSG_BindRequestMsg;
  ASSERT_TRUE( stunlib_isRequest(&msg) );

}



CTEST(turnclient, isTurnChan)
{
  unsigned char req[] =
    "Kinda buffer ready to be overwritten";

  /* We overwrite some of the data, but in this case who cares.. */
  stunlib_encodeTurnChannelNumber(0x4001,
                                  sizeof(req),
                                  (uint8_t*)req);

  ASSERT_TRUE( stunlib_isTurnChannelData(req) );

}



CTEST(turnclient, GetErrorReason)
{
  ASSERT_TRUE( 0 == strcmp(stunlib_getErrorReason(3, 0),"Try Alternate") );
  ASSERT_TRUE( 0 == strcmp(stunlib_getErrorReason(4, 0),"Bad Request") );
  ASSERT_TRUE( 0 == strcmp(stunlib_getErrorReason(4, 1),"Unauthorized") );
  ASSERT_TRUE( 0 == strcmp(stunlib_getErrorReason(4, 20),"Unknown Attribute") );
  ASSERT_TRUE( 0 == strcmp(stunlib_getErrorReason(4, 30),"Stale Credentials") );
  ASSERT_TRUE( 0 ==
               strcmp(stunlib_getErrorReason(4,
                                             31),"Integrity Check Failure") );
  ASSERT_TRUE( 0 == strcmp(stunlib_getErrorReason(4, 32),"Missing Username") );
  ASSERT_TRUE( 0 == strcmp(stunlib_getErrorReason(4, 37),"No Binding") );
  ASSERT_TRUE( 0 == strcmp(stunlib_getErrorReason(4, 38),"Stale Nonce") );
  ASSERT_TRUE( 0 == strcmp(stunlib_getErrorReason(4, 41),"Wrong Username") );
  ASSERT_TRUE( 0 ==
               strcmp(stunlib_getErrorReason(4,
                                             42),
                      "Unsupported Transport Protocol") );
  ASSERT_TRUE( 0 == strcmp(stunlib_getErrorReason(5, 00),"Server Error") );
  ASSERT_TRUE( 0 == strcmp(stunlib_getErrorReason(6, 00),"Global Failure") );
  ASSERT_TRUE( 0 ==
               strcmp(stunlib_getErrorReason(4,
                                             86),"Allocation Quota Reached") );
  ASSERT_TRUE( 0 ==
               strcmp(stunlib_getErrorReason(5, 8),"Insufficient Capacity") );
  ASSERT_TRUE( 0 == strcmp(stunlib_getErrorReason(4, 87),"Role Conflict") );

  ASSERT_TRUE( 0 == strcmp(stunlib_getErrorReason(2, 43),"???") );

}
