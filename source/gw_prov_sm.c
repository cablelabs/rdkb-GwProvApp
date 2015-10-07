/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2015 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

/**********************************************************************
   Copyright [2014] [Cisco Systems, Inc.]
 
   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at
 
       http://www.apache.org/licenses/LICENSE-2.0
 
   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
**********************************************************************/

#define _GW_PROV_SM_C_

/*! \file gw_prov_sm.c
    \brief gw provisioning
*/

/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/

#include <arpa/inet.h>
#include <errno.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <ruli.h>
#include <sysevent/sysevent.h>
#include <syscfg/syscfg.h>
#include <pthread.h>
#include "sys_types.h"
#include "sys_nettypes.h"
//#include "sys_utils.h"
#include "gw_prov_abstraction.h"


/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/

#define ERNETDEV_MODULE "/fss/gw/lib/modules/2.6.39.3/drivers/net/erouter_ni.ko"
#define NETUTILS_IPv6_GLOBAL_ADDR_LEN     	 128
#define ER_NETDEVNAME "erouter0"
#define IFNAME_WAN_0    "wan0"
#define IFNAME_ETH_0    "eth0"

/*! New implementation*/

#define ARGV_NOT_EXIST 0
#define ARGV_DISABLED 1
#define ARGV_ENABLED 3

#define INFINITE_LIFE_TIME 0xFFFFFFFF
#define MAX_CFG_PATH_LEN 256
#define MAX_CMDLINE_LEN 255

#define DOCSIS_MULTICAST_PROC_MDFMODE "/proc/net/dbrctl/mdfmode"
#define DOCSIS_MULTICAST_PROC_MDFMODE_ENABLED "Enable"


typedef struct _GwTlv2ChangeFlags
{
    char EnableCWMP_modified;
    char URL_modified;
    char Username_modified;
    char Password_modified;
    char ConnectionRequestUsername_modified;
    char ConnectionRequestPassword_modified;
    char AcsOverride_modified;
}GwTlv2ChangeFlags_t;

/*Structure of local internal data */
typedef struct _GwTlvsLocalDB
{
    GwTlv2StructExtIf_t tlv2;
    GwTlv2ChangeFlags_t tlv2_flags;
}GwTlvsLocalDB_t;

/* New implementation !*/



/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/

/*! New implementation */
static void GW_Local_PrintHexStringToStderr(Uint8 *str, Uint16 len);
static void GW_SetTr069PaMibBoolean(Uint8 **cur, Uint8 sub_oid, Uint8 value);
static void GW_SetTr069PaMibString(Uint8 **cur, Uint8 sub_oid, Uint8* value);
static STATUS GW_TlvParserInit(void);
//static TlvParseCallbackStatus_e GW_SetTr069PaCfg(Uint8 type, Uint16 length, const Uint8* value);
static TlvParseCallbackStatusExtIf_e GW_Tr069PaSubTLVParse(Uint8 type, Uint16 length, const Uint8* value);
static STATUS GW_SetTr069PaDataInTLV11Buffer(Uint8* buf, Int* len);
static STATUS GW_UpdateTr069Cfg(void);

//static TlvParseCallbackStatus_e gotEnableType(Uint8 type, Uint16 length, const Uint8* value);

/* New implementation !*/
static void LAN_start();

/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/


static int snmp_inited = 0;
static int netids_inited = 0;
static int gDocTftpOk = 0;
static int hotspot_started = 0;
static int lan_telnet_started = 0;
static int ciscoconnect_started = 0;
static int webui_started = 0;
static Uint32 factory_mode = 0;


static DOCSIS_Esafe_Db_extIf_e eRouterMode = DOCESAFE_ENABLE_DISABLE_extIf;
static DOCSIS_Esafe_Db_extIf_e oldRouterMode;
static int sysevent_fd;
static token_t sysevent_token;
static pthread_t sysevent_tid;
static int phylink_wan_state = 0;
static int bridge_mode = 0;

static GwTlvsLocalDB_t gwTlvsLocalDB;

/**************************************************************************/
/*      LOCAL FUNCTIONS:                                                  */
/**************************************************************************/


/**************************************************************************/
/*! \fn STATUS GW_TlvParserInit(void)
 **************************************************************************
 *  \brief Initialize before the parsing
 *  \return Initialization status: OK/NOK
 **************************************************************************/
static STATUS GW_TlvParserInit(void)
{
    /*Initialize local DB*/
    // GW_FreeTranstAddrAccessList();
    memset(&gwTlvsLocalDB, 0, sizeof(gwTlvsLocalDB));

    /*Open the SNMP response socket*/
    // GW_CreateSnmpResponseSocket();
    /*Init SNMP TLV's default values*/
    // GW_InitSNMPTlvsDefaults();

    return STATUS_OK;
}

static void GW_Local_PrintHexStringToStderr(Uint8 *str, Uint16 len)
 {
    int i; 

    fprintf(stderr, "hex string = '");
    for(i=0; i<len; i++) 
    {
        fprintf(stderr, "%02X", str[i]);
    }
    fprintf(stderr, "'\n");
 }

static TlvParseCallbackStatusExtIf_e GW_Tr069PaSubTLVParse(Uint8 type, Uint16 length, const Uint8* value)
{
    switch(type)
    {
        case GW_SUBTLV_TR069_ENABLE_CWMP_EXTIF:
            if ((int)(*value) == 0 || (int)(*value) == 1) {
                gwTlvsLocalDB.tlv2.EnableCWMP = (GwTr069PaEnableCwmpTypeExtIf_e)(*value);
                gwTlvsLocalDB.tlv2_flags.EnableCWMP_modified = 1;
            }
            else gwTlvsLocalDB.tlv2.EnableCWMP = GW_TR069_ENABLE_CWMP_FALSE_EXTIF;
            break;

        case GW_SUBTLV_TR069_URL_EXTIF:
            if (length <= GW_TR069_TLV_MAX_URL_LEN) 
            {
                memcpy(gwTlvsLocalDB.tlv2.URL, value, length);
                gwTlvsLocalDB.tlv2.URL[length] = '\0';
                gwTlvsLocalDB.tlv2_flags.URL_modified = 1;
            }
            else gwTlvsLocalDB.tlv2.URL[0] = '\0';
            break;

        case GW_SUBTLV_TR069_USERNAME_EXTIF:
            if (length <= GW_TR069_TLV_MAX_USERNAME_LEN) 
            {
                memcpy(gwTlvsLocalDB.tlv2.Username, value, length);
                gwTlvsLocalDB.tlv2.Username[length] = '\0';
                gwTlvsLocalDB.tlv2_flags.Username_modified = 1;
            }
            else gwTlvsLocalDB.tlv2.Username[0] = '\0';
            break;

        case GW_SUBTLV_TR069_PASSWORD_EXTIF:
            if (length <= GW_TR069_TLV_MAX_PASSWORD_LEN) 
            {
                memcpy(gwTlvsLocalDB.tlv2.Password, value, length);
                gwTlvsLocalDB.tlv2.Password[length] = '\0';
                gwTlvsLocalDB.tlv2_flags.Password_modified = 1;
            }
            else gwTlvsLocalDB.tlv2.Password[0] = '\0'; 
            break;

        case GW_SUBTLV_TR069_CONNREQ_USERNAME_EXTIF:
            if (length <= GW_TR069_TLV_MAX_USERNAME_LEN) 
            {
                memcpy(gwTlvsLocalDB.tlv2.ConnectionRequestUsername, value, length);
                gwTlvsLocalDB.tlv2.ConnectionRequestUsername[length] = '\0';
                gwTlvsLocalDB.tlv2_flags.ConnectionRequestUsername_modified = 1;
            }
            else gwTlvsLocalDB.tlv2.ConnectionRequestUsername[0] = '\0';
            break;

        case GW_SUBTLV_TR069_CONNREQ_PASSWORD_EXTIF:
            if (length <= GW_TR069_TLV_MAX_PASSWORD_LEN) 
            {
                memcpy(gwTlvsLocalDB.tlv2.ConnectionRequestPassword, value, length);
                gwTlvsLocalDB.tlv2.ConnectionRequestPassword[length] = '\0';
                gwTlvsLocalDB.tlv2_flags.ConnectionRequestPassword_modified = 1;
            }
            else gwTlvsLocalDB.tlv2.ConnectionRequestPassword[0] = '\0';
            break;

        case GW_SUBTLV_TR069_ACS_OVERRIDE_EXTIF:
            if ((int)(*value) == 0 || (int)(*value) == 1) {
                gwTlvsLocalDB.tlv2.ACSOverride = (GwTr069PaAcsOverrideTypeExtIf_e)(*value);
                gwTlvsLocalDB.tlv2_flags.AcsOverride_modified = 1;
            }
            else gwTlvsLocalDB.tlv2.ACSOverride = GW_TR069_ACS_OVERRIDE_DISABLED_EXTIF;
            break;

        default:
            printf("Unknown Sub TLV In TLV 2");
            break;
    }

    return TLV_PARSE_CALLBACK_OK_EXTIF;
}

// All MIB entries in hex are: 30 total_len oid_base oid_value 00 data_type data_len data

// Oid_Base = 1.3.6.1.4.1.1429.79.6.1
static Uint8 GW_Tr069PaMibOidBase[12] = { 0x06, 0x0c, 0x2b, 0x06, 0x01, 0x04, 0x01, 0x8b, 0x15, 0x4f, 0x06, 0x01 }; 

/* TR-069 MIB SUB OIDs */
#define GW_TR069_MIB_SUB_OID_ENABLE_CWMP                 0x01
#define GW_TR069_MIB_SUB_OID_URL                         0x02
#define GW_TR069_MIB_SUB_OID_USERNAME                    0x03
#define GW_TR069_MIB_SUB_OID_PASSWORD                    0x04
#define GW_TR069_MIB_SUB_OID_CONNREQ_USERNAME            0x05
#define GW_TR069_MIB_SUB_OID_CONNREQ_PASSWORD            0x06
#define GW_TR069_MIB_SUB_OID_ALLOW_DOCSIS_CONFIG         0x09  // not implemented yet - 03/31/2014

/* TR-069 MIB OID INSTANCE NUM */
#define GW_TR069_MIB_SUB_OID_INSTANCE_NUM                0x00

/* TR-069 MIB DATA TYPE */
#define GW_TR069_MIB_DATATYPE_BOOL                       0x02
#define GW_TR069_MIB_DATATYPE_STRING                     0x04

/* TR-069 MIB DATA TYPE LENGTH */
#define GW_TR069_MIB_DATATYPE_LEN_BOOL                   0x01

static void GW_SetTr069PaMibBoolean(Uint8 **cur, Uint8 sub_oid, Uint8 value)
{
    Uint8 *mark;
    Uint8 *current = *cur;

    // SEQUENCE (0x30); Skip total length (1-byte, to be filled later)
    *(current++) = 0x30; current++; mark = current; 
    memcpy(current, GW_Tr069PaMibOidBase, 12);  current += 12;  
    *(current++) = sub_oid;
    *(current++) = GW_TR069_MIB_SUB_OID_INSTANCE_NUM;
    *(current++) = GW_TR069_MIB_DATATYPE_BOOL; 
    *(current++) = GW_TR069_MIB_DATATYPE_LEN_BOOL;
    *(current++) = value;
    *(mark-1) = (Uint8)(current - mark);

    *cur = current;
}

static void GW_SetTr069PaMibString(Uint8 **cur, Uint8 sub_oid, Uint8* value)
{
    Uint8 *mark;
    Uint8 *current = *cur;

    // SEQUENCE (0x30); Skip total length (1-byte, to be filled later)
    *(current++) = 0x30; current++; mark = current; 
    memcpy(current, GW_Tr069PaMibOidBase, 12);  current += 12;  
    *(current++) = sub_oid;
    *(current++) = GW_TR069_MIB_SUB_OID_INSTANCE_NUM;
    *(current++) = GW_TR069_MIB_DATATYPE_STRING; 
    *(current++) = (Uint8)strlen(value);
    if(*(current-1)) { memcpy(current, value, *(current-1)); current += *(current-1);}
    *(mark-1) = (Uint8)(current - mark);

    *cur = current;
}

static STATUS GW_SetTr069PaDataInTLV11Buffer(Uint8* buf, Int* len)
{
    Uint8 *ptr = buf;

    // EnableCWMP
    if(gwTlvsLocalDB.tlv2_flags.EnableCWMP_modified)
        GW_SetTr069PaMibBoolean(&ptr, GW_TR069_MIB_SUB_OID_ENABLE_CWMP, (Uint8)(gwTlvsLocalDB.tlv2.EnableCWMP));

    // URL
    if(gwTlvsLocalDB.tlv2_flags.URL_modified)
        GW_SetTr069PaMibString(&ptr, GW_TR069_MIB_SUB_OID_URL, (Uint8*)(gwTlvsLocalDB.tlv2.URL));

    // Username
    if(gwTlvsLocalDB.tlv2_flags.Username_modified)
        GW_SetTr069PaMibString(&ptr, GW_TR069_MIB_SUB_OID_USERNAME, (Uint8*)(gwTlvsLocalDB.tlv2.Username));

    // Password
    if(gwTlvsLocalDB.tlv2_flags.Password_modified)
        GW_SetTr069PaMibString(&ptr, GW_TR069_MIB_SUB_OID_PASSWORD, (Uint8*)(gwTlvsLocalDB.tlv2.Password));

    // ConnectionRequestUsername
    if(gwTlvsLocalDB.tlv2_flags.ConnectionRequestUsername_modified)
        GW_SetTr069PaMibString(&ptr, GW_TR069_MIB_SUB_OID_CONNREQ_USERNAME, (Uint8*)(gwTlvsLocalDB.tlv2.ConnectionRequestUsername));

    // ConnectRequestPassword
    if(gwTlvsLocalDB.tlv2_flags.ConnectionRequestPassword_modified)
        GW_SetTr069PaMibString(&ptr, GW_TR069_MIB_SUB_OID_CONNREQ_PASSWORD, (Uint8*)(gwTlvsLocalDB.tlv2.ConnectionRequestPassword));

    // ACSOverride (corresponds to MIB saTr069ClientAllowDocsisConfig) is not implmented yet, RTian 04/07/2014
#if (0)  
    // ACSOverride
    //TLV11 saTR069ClientAllowDocsisConfig is opposite of ACSOverride
    if(gwTlvsLocalDB.tlv2_flags.AcsOverride_modified)
        GW_SetTr069PaMibBoolean(&ptr, GW_TR069_MIB_SUB_OID_ALLOW_DOCSIS_CONFIG, (Uint8)(gwTlvsLocalDB.tlv2.ACSOverride ? 0x00 : 0x01));
#endif

    *len = ptr - buf;

    return STATUS_OK;
}

#define SNMP_DATA_BUF_SIZE 1000

static STATUS GW_UpdateTr069Cfg(void)
{
    /* SNMP TLV's data buffer*/
    Uint8 Snmp_Tlv11Buf[SNMP_DATA_BUF_SIZE];
    Int Snmp_Tlv11BufLen = 0;
    STATUS ret = STATUS_OK;

    /*Init the data buffer*/
    memset(Snmp_Tlv11Buf, 0, SNMP_DATA_BUF_SIZE);

    /*Convert TLV 202.2 data into TLV11 data*/
    GW_SetTr069PaDataInTLV11Buffer(Snmp_Tlv11Buf, &Snmp_Tlv11BufLen);

    /*
    fprintf(stderr, "<RT> %s - Snmp \n", __FUNCTION__);
    GW_Local_PrintHexStringToStderr(Snmp_Tlv11Buf, Snmp_Tlv11BufLen);
    */

    
  
    /*Send TLV11 data to SNMP Agent*/
    if(Snmp_Tlv11BufLen)
    {
        ret = sendTLV11toSnmpAgent((void *)Snmp_Tlv11Buf, (int)Snmp_Tlv11BufLen );
        
    }
return ret;
#if 0
        SnmpaIfResponse_t *tlv11Resp = (SnmpaIfResponse_t*)malloc(sizeof(SnmpaIfResponse_t)+sizeof(int));
        if (!tlv11Resp)
        {
            LOG_GW_ERROR("Failed to allocate dynamic memory");
            goto label_nok;
        }
        memset(tlv11Resp, 0, sizeof(SnmpaIfResponse_t)+sizeof(int));

        /* Set TLV11 whitin whole config file and TLV11 duplication test */
        ret = (STATUS)SNMPAIF_SetTLV11Config(SNMP_AGENT_CTRL_SOCK, (void *)Snmp_Tlv11Buf, (int)Snmp_Tlv11BufLen, tlv11Resp);

        if(tlv11Resp->len >= sizeof(int))
        {
            Int32 errorCode = 0;
            memcpy(&errorCode, tlv11Resp->value, sizeof(int));
            /*Need to send the required event*/
            // ReportTlv11Events(errorCode);
            LOG_GW_ERROR("Failed to set TLV11 parameters - error code = %d", errorCode);
            // fprintf(stderr, "<RT> %s - Failed to set TLV11 parameters - error code = %d\n", __FUNCTION__, errorCode);
        }
   
        if(ret != STATUS_OK)
        {
#if (SA_CUSTOM)
            LOG_GW_ERROR("TLV11 internal SNMP set failed! IGNORING...");
#else //TI Org
            LOG_GW_ERROR("TLV11 internal SNMP set failed!");
            if(tlv11Resp) free(tlv11Resp);
            goto label_nok;
#endif
        }

        if(tlv11Resp) free(tlv11Resp);
    }

    return STATUS_OK;

label_nok:
    return STATUS_NOK;
#endif 
}

/**************************************************************************/
/*! \fn static STATUS GWP_SysCfgGetInt
 **************************************************************************
 *  \brief Get Syscfg Integer Value
 *  \return int/-1
 **************************************************************************/
static int GWP_SysCfgGetInt(const char *name)
{
   char out_value[20];
   int outbufsz = sizeof(out_value);

   if (!syscfg_get(NULL, name, out_value, outbufsz))
   {
      return atoi(out_value);
   }
   else
   {
      return -1;
   }
}

/**************************************************************************/
/*! \fn static STATUS GWP_SysCfgSetInt
 **************************************************************************
 *  \brief Set Syscfg Integer Value
 *  \return 0:success, <0: failure
 **************************************************************************/
static int GWP_SysCfgSetInt(const char *name, int int_value)
{
   char value[20];
   sprintf(value, "%d", int_value);

   return syscfg_set(NULL, name, value);
}

/**************************************************************************/
/*! \fn static STATUS GWP_UpdateEsafeAdminMode()
 **************************************************************************
 *  \brief Update esafeAdminMode
 *  \return OK/NOK
 **************************************************************************/
static STATUS GWP_UpdateEsafeAdminMode(DOCSIS_Esafe_Db_extIf_e enableMode)
{
    
    eSafeDevice_Enable(enableMode);

    return STATUS_OK;
}

/**************************************************************************/
/*! \fn Bool GWP_IsGwEnabled(void)
 **************************************************************************
 *  \brief Is gw enabled
 *  \return True/False
**************************************************************************/
static Bool GWP_IsGwEnabled(void)
{
    
    if (eRouterMode == DOCESAFE_ENABLE_DISABLE_extIf)
    {
        return False;
    }
    else
    {
        return True;
    }
}



void docsis_gotEnable_callback(Uint8 state)
{

   eRouterMode = state;

}
/**************************************************************************/
/*! \fn void GWP_DocsisInited(void)
 **************************************************************************
 *  \brief Actions when DOCSIS is initialized
 *  \return None
**************************************************************************/
static void GWP_DocsisInited(void)
{
    macaddr_t macAddr;
   
    /* Initialize docsis interface */
    initializeDocsisInterface();

    /* Register the eRouter  */
    
    	getNetworkDeviceMacAddress(&macAddr);
    	
    
    eSafeDevice_Initialize(&macAddr);
    
       
    eSafeDevice_SetProvisioningStatusProgress(ESAFE_PROV_STATE_NOT_INITIATED_extIf);
	
     /* Add paths */
     
     eSafeDevice_AddeRouterPhysicalNetworkInterface(IFNAME_ETH_0, True);
           
     eSafeDevice_AddeRouterPhysicalNetworkInterface("usb0",True);

    /* Register on more events */
    registerDocsisEvents();
    
    if(factory_mode)
        LAN_start();

}


/**************************************************************************/
/*! \fn void GWP_EnableERouter(void)
 **************************************************************************
 *  \brief Actions enable eRouter
 *  \return None
**************************************************************************/
static void GWP_EnableERouter(void)
{
    /* Update ESAFE state */
    GWP_UpdateEsafeAdminMode(eRouterMode);

    eSafeDevice_SetErouterOperationMode(DOCESAFE_EROUTER_OPER_NOIPV4_NOIPV6_extIf);
    
    eSafeDevice_SetProvisioningStatusProgress(ESAFE_PROV_STATE_IN_PROGRESS_extIf);
	
    bridge_mode = 0;
    system("sysevent set bridge_mode 0");
    system("sysevent set forwarding-restart");

    printf("******************************");
    printf("* Enabled (after cfg file)  *");
    printf("******************************");
}

static void GWP_EnterRouterMode(void)
{
    
    if (eRouterMode == DOCESAFE_ENABLE_DISABLE_extIf)
         return;
    
    bridge_mode = 0;
    system("sysevent set bridge_mode 0");
    system("sysevent set forwarding-restart");
}

/**************************************************************************/
/*! \fn void GWP_DisableERouter(void)
 **************************************************************************
 *  \brief Actions disable eRouter
 *  \return None
**************************************************************************/
static void GWP_DisableERouter(void)
{
    /* Update ESAFE state */
    GWP_UpdateEsafeAdminMode(eRouterMode);

    eSafeDevice_SetErouterOperationMode(DOCESAFE_EROUTER_OPER_DISABLED_extIf);
    
    /* Reset Switch, to remove all VLANs */ 
    eSafeDevice_SetProvisioningStatusProgress(ESAFE_PROV_STATE_NOT_INITIATED_extIf);
   	
    char sysevent_cmd[80];
    snprintf(sysevent_cmd, sizeof(sysevent_cmd), "sysevent set bridge_mode %d", bridge_mode);
    system(sysevent_cmd);
    system("sysevent set forwarding-restart");

    printf("******************************");
    printf("* Disabled (after cfg file)  *");
    printf("******************************");
}

static void GWP_EnterBridgeMode(void)
{
    system("sysevent set bridge_mode 1");
    system("sysevent set forwarding-restart");
}

/**************************************************************************/
/*! \fn void GWP_UpdateERouterMode(void)
 **************************************************************************
 *  \brief Actions when ERouter Mode is Changed
 *  \return None
**************************************************************************/
static void GWP_UpdateERouterMode(void)
{
    // This function is called when TLV202 is received with a valid Router Mode
    // It could trigger a mode switch but user can still override it...
    printf("%s: %d->%d\n", __func__, oldRouterMode, eRouterMode);
    if (oldRouterMode != eRouterMode)
    {
        GWP_SysCfgSetInt("last_erouter_mode", eRouterMode);  // save the new mode only
        syscfg_commit();

        
        if (eRouterMode == DOCESAFE_ENABLE_DISABLE_extIf)
        {
            // This means we are switching from router mode to bridge mode, set bridge_mode
            // to 2 since user did not specify it
            bridge_mode = 2;
            GWP_DisableERouter();
        }
        else
        {
            // TLV202 allows eRouter, but we still need to check user's preference
            bridge_mode = GWP_SysCfgGetInt("bridge_mode");
            if (bridge_mode == 1 || bridge_mode == 2)
            {
                // erouter disabled by user, keep it disabled
                //mipieper -- dont disable erouter on bridge mode 
                //eRouterMode = DOCESAFE_ENABLE_DISABLE;
            }
            else if (oldRouterMode == DOCESAFE_ENABLE_DISABLE_extIf) // from disable to enable
            {
                GWP_EnableERouter();
            }
            else  // remain enabled, switch mode
            {
                /* Update ESAFE state */
                GWP_UpdateEsafeAdminMode(eRouterMode);

                system("sysevent set erouter_mode-updated");
            }
        }
    }
}

/**************************************************************************/
/*! \fn void GWP_ProcessUtopiaRestart(void)
 **************************************************************************
 *  \brief Actions when GUI request restarting of Utopia (bridge mode changes)
 *  \return None
**************************************************************************/
static void GWP_ProcessUtopiaRestart(void)
{
    // This function is called when "system-restart" event is received, This
    // happens when WEBUI change bridge configuration. We do not restart the
    // whole system, only routing/bridging functions only
 
    // TODO:mipieper, figure out how to handle bridge mode transition
    //system("sysevent set forwarding-stop");

    bridge_mode = GWP_SysCfgGetInt("bridge_mode");
    eRouterMode = GWP_SysCfgGetInt("last_erouter_mode");

    printf("bridge_mode = %d, erouter_mode = %d\n", bridge_mode, eRouterMode);

    if (bridge_mode == 1 || bridge_mode == 2)
    {
        
        GWP_EnterBridgeMode();
    }
    else if (eRouterMode == DOCESAFE_ENABLE_DISABLE_extIf) // TLV202 only allows bridge mode
    {
        bridge_mode = 2;
        GWP_EnterBridgeMode();
    }
    else
    {
        GWP_EnterRouterMode();
    }
}

/**************************************************************************/
/*! \fn int GWP_ProcessIpv4Down();
 **************************************************************************
 *  \brief IPv4 WAN Side Routing - Exit
 *  \return 0
**************************************************************************/
static int GWP_ProcessIpv4Down(void)
{
    esafeErouterOperModeExtIf_e operMode;

    /* Set operMode */
    
    eSafeDevice_GetErouterOperationMode(&operMode);
    
    if (operMode == DOCESAFE_EROUTER_OPER_IPV4_IPV6_extIf)
    {
        /* Now we have both --> Go to v6 only */
        operMode = DOCESAFE_EROUTER_OPER_IPV6_extIf;
    }
    else
    {
        /* Only v4 --> Neither */
        operMode = DOCESAFE_EROUTER_OPER_NOIPV4_NOIPV6_extIf;
    }
    
    eSafeDevice_SetErouterOperationMode(operMode);

    return 0;
}

/**************************************************************************/
/*! \fn int GWP_ProcessIpv4Up
 **************************************************************************
 *  \brief IPv4 WAN Side Routing
 *  \return 0
**************************************************************************/
static int GWP_ProcessIpv4Up(void)
{
    esafeErouterOperModeExtIf_e operMode;

    /*update esafe db with router provisioning status*/
    eSafeDevice_SetProvisioningStatusProgress(ESAFE_PROV_STATE_FINISHED_extIf);

    /* Set operMode */
    eSafeDevice_GetErouterOperationMode(&operMode);
    if (operMode == DOCESAFE_EROUTER_OPER_IPV6_extIf)
    {
        /* Now we have both */
        operMode = DOCESAFE_EROUTER_OPER_IPV4_IPV6_extIf;
    }
    else
    {
        /* Only v4 */
        operMode = DOCESAFE_EROUTER_OPER_IPV4_extIf;
    }
    eSafeDevice_SetErouterOperationMode(operMode);

    printf("******************************");
    printf("*        IPv4 Routing        *");
    printf("******************************");

    return 0;
}

/**************************************************************************/
/*! \fn int GWP_ProcessIpV6Down()
 **************************************************************************
 *  \brief IPv6 WAN Side Routing - Exit
 *  \return 0
**************************************************************************/
static int GWP_ProcessIpv6Down(void)
{
    esafeErouterOperModeExtIf_e operMode;

    /* Set operMode */
    eSafeDevice_GetErouterOperationMode(&operMode);
    if (operMode == DOCESAFE_EROUTER_OPER_IPV4_IPV6_extIf)
    {
        /* Now we have both --> Go to v4 only */
        operMode = DOCESAFE_EROUTER_OPER_IPV4_extIf;
    }
    else
    {
        /* Only v6 --> Neither */
        operMode = DOCESAFE_EROUTER_OPER_NOIPV4_NOIPV6_extIf;
    }
    
    eSafeDevice_SetErouterOperationMode(operMode);

    return 0;
}

/**************************************************************************/
/*! \fn int GWP_ProcessIpV6Up()
 **************************************************************************
 *  \brief IPv6 WAN Side Routing
 *  \param[in] SME Handler params
 *  \return 0
**************************************************************************/
static int GWP_ProcessIpv6Up(void)
{
    esafeErouterOperModeExtIf_e operMode;

    /*update esafe db with router provisioning status*/
    
    eSafeDevice_SetProvisioningStatusProgress(ESAFE_PROV_STATE_FINISHED_extIf);
    

    /* Set operMode */
    eSafeDevice_GetErouterOperationMode(&operMode);
    
    if (operMode == DOCESAFE_EROUTER_OPER_IPV4_extIf)
    {
        /* Now we have both */
        operMode = DOCESAFE_EROUTER_OPER_IPV4_IPV6_extIf;
        
    }
    else
    {
        /* Only v6 */
        operMode = DOCESAFE_EROUTER_OPER_IPV6_extIf;
    }
    eSafeDevice_SetErouterOperationMode(operMode);


    printf("******************************");
    printf("*        IPv6 Routing        *");
    printf("******************************");

    return 0;
}

/**************************************************************************/
/*! \fn void *GWP_sysevent_threadfunc(void *data)
 **************************************************************************
 *  \brief Function to process sysevent event
 *  \return 0
**************************************************************************/
static void *GWP_sysevent_threadfunc(void *data)
{
    async_id_t erouter_mode_asyncid;
    async_id_t ipv4_status_asyncid;
    async_id_t ipv6_status_asyncid;
    async_id_t system_restart_asyncid;
    async_id_t snmp_subagent_status_asyncid;
    async_id_t primary_lan_l3net_asyncid;
    async_id_t lan_status_asyncid;
    async_id_t bridge_status_asyncid;
    async_id_t ipv6_dhcp_asyncid;
    
    
    sysevent_setnotification(sysevent_fd, sysevent_token, "erouter_mode", &erouter_mode_asyncid);
    sysevent_setnotification(sysevent_fd, sysevent_token, "ipv4-status",  &ipv4_status_asyncid);
    sysevent_setnotification(sysevent_fd, sysevent_token, "ipv6-status",  &ipv6_status_asyncid);
    sysevent_setnotification(sysevent_fd, sysevent_token, "system-restart",  &system_restart_asyncid);
    sysevent_setnotification(sysevent_fd, sysevent_token, "snmp_subagent-status",  &snmp_subagent_status_asyncid);
    sysevent_setnotification(sysevent_fd, sysevent_token, "primary_lan_l3net",  &primary_lan_l3net_asyncid);
    sysevent_setnotification(sysevent_fd, sysevent_token, "lan-status",  &lan_status_asyncid);
    sysevent_setnotification(sysevent_fd, sysevent_token, "bridge-status",  &bridge_status_asyncid);
    sysevent_setnotification(sysevent_fd, sysevent_token, "tr_" ER_NETDEVNAME "_dhcpv6_client_v6addr",  &ipv6_status_asyncid);

    sysevent_set_options(sysevent_fd, sysevent_token, "system-restart", TUPLE_FLAG_EVENT);

    for (;;)
    {
        char name[25], val[42], buf[10];
        int namelen = sizeof(name);
        int vallen  = sizeof(val);
        int err;
        async_id_t getnotification_asyncid;

        err = sysevent_getnotification(sysevent_fd, sysevent_token, name, &namelen,  val, &vallen, &getnotification_asyncid);

        if (err)
        {
           printf("%s-ERR: %d\n", __func__, err);
        }
        else
        {
            if (strcmp(name, "erouter_mode")==0)
            {
                oldRouterMode = eRouterMode;
                eRouterMode = atoi(val);

                if (eRouterMode != DOCESAFE_ENABLE_DISABLE_extIf &&
                    eRouterMode != DOCESAFE_ENABLE_IPv4_extIf    &&
                    eRouterMode != DOCESAFE_ENABLE_IPv6_extIf    &&
                    eRouterMode != DOCESAFE_ENABLE_IPv4_IPv6_extIf)
                {
                    eRouterMode = DOCESAFE_ENABLE_DISABLE_extIf;
                }

                GWP_UpdateERouterMode();
            }
            else if (strcmp(name, "ipv4-status") == 0)
            {
                if (strcmp(val, "up")==0)
                {
                    GWP_ProcessIpv4Up();
                }
                else if (strcmp(val, "down")==0)
                {
                    GWP_ProcessIpv4Down();
                }
            }
            else if (strcmp(name, "ipv6-status") == 0)
            {
                if (strcmp(val, "up")==0)
                {
                    GWP_ProcessIpv6Up();
                }
                else if (strcmp(val, "down")==0)
                {
                    GWP_ProcessIpv6Down();
                }
            }
            else if (strcmp(name, "system-restart") == 0)
            {
                printf("gw_prov_sm: got system restart\n");
                GWP_ProcessUtopiaRestart();
            } 
            else if (strcmp(name, "snmp_subagent-status") == 0 && !snmp_inited)
            {
                snmp_inited = 1;
                if (netids_inited) {
                    if(!factory_mode)
                        LAN_start();
                }
            } 
            else if (strcmp(name, "primary_lan_l3net") == 0)
            {
                if (snmp_inited)
                    LAN_start();
                netids_inited = 1;
            }
            else if (strcmp(name, "lan-status") == 0 || strcmp(name, "bridge-status") == 0 ) 
            {
                if (strcmp(val, "started") == 0) {
                    if (!webui_started) { 
                        startWebUIProcess();
                        webui_started = 1 ;
#ifdef CONFIG_CISCO_XHS
                        //Piggy back off the webui start event to signal XHS startup
                        sysevent_get(sysevent_fd, sysevent_token, "homesecurity_lan_l3net", buf, sizeof(buf));
                        if (buf[0] != '\0') sysevent_set(sysevent_fd, sysevent_token, "ipv4-up", buf, 0);
#endif
                    }
                    
                    if (!hotspot_started) { 
                        sysevent_set(sysevent_fd, sysevent_token, "hotspot-start", "", 0);
                        hotspot_started = 1 ;
                    }
                    
                    if (factory_mode && lan_telnet_started == 0) {
                        system("/usr/sbin/telnetd -l /usr/sbin/cli -i brlan0");
                        lan_telnet_started=1;
                    }
#ifdef CONFIG_CISCO_FEATURE_CISCOCONNECT

                    if (!ciscoconnect_started) { 
                        sysevent_set(sysevent_fd, sysevent_token, "ciscoconnect-restart", "", 0);
                        ciscoconnect_started = 1 ;
                    }
#endif
                }
            } else if (strcmp(name, "tr_" ER_NETDEVNAME "_dhcpv6_client_v6addr") == 0) {
                Uint8 v6addr[ NETUTILS_IPv6_GLOBAL_ADDR_LEN / sizeof(Uint8) ];
                Uint8 soladdr[ NETUTILS_IPv6_GLOBAL_ADDR_LEN / sizeof(Uint8) ];
                inet_pton(AF_INET6, val, v6addr);
                
                getMultiCastGroupAddress(v6addr,soladdr);
                inet_ntop(AF_INET6, soladdr, val, sizeof(val));
                
                
                sysevent_set(sysevent_fd, sysevent_token, "ipv6_"ER_NETDEVNAME"_dhcp_solicNodeAddr", val,0);
                sysevent_set(sysevent_fd, sysevent_token, "firewall-restart", "",0);
            }
        }
    }
    return 0;
}




/**************************************************************************/
/*! \fn int GWP_act_DocsisLinkDown(SME_APP_T *app, SME_EVENT_T *event);
 **************************************************************************
 *  \brief Actions required upon linkDown from ActiveProvisioned
 *  \param[in] SME Handler params
 *  \return 0
**************************************************************************/
static int GWP_act_DocsisLinkDown_callback_1()
{
    phylink_wan_state = 0;
    system("sysevent set phylink_wan_state down");
   
    printf("\n**************************\n");
    printf("\nsysevent set phylink_wan_state down\n");
    printf("\n**************************\n\n");
    return 0;
}

static int GWP_act_DocsisLinkDown_callback_2()
{
    
    if (eRouterMode != DOCESAFE_ENABLE_DISABLE_extIf)
    {
       printf("Stopping wan service\n");
       system("sysevent set wan-stop");
    }

    return 0;
}


static int GWP_act_DocsisLinkUp_callback()
{
    phylink_wan_state = 1;
    system("sysevent set phylink_wan_state up");
    printf("\n**************************\n");
    printf("\nsysevent set phylink_wan_state up\n");
    printf("\n**************************\n\n");

    
    if (eRouterMode != DOCESAFE_ENABLE_DISABLE_extIf && bridge_mode == 0)
    {
        printf("Starting wan service\n");
        system("sysevent set wan-start");
    }

    return 0;
}


/**************************************************************************/
/*! \fn int GWP_act_DocsisCfgfile(SME_APP_T *app, SME_EVENT_T *event);
 **************************************************************************
 *  \brief Parse Config File
 *  \param[in] SME Handler params
 *  \return 0
**************************************************************************/
static int GWP_act_DocsisCfgfile_callback(Char* cfgFile)
{
    Char *cfgFileName = NULL;
    struct stat cfgFileStat;
    Uint8 *cfgFileBuff = NULL;
    Uint32 cfgFileBuffLen;
    int cfgFd;
    ssize_t actualNumBytes;
    //TlvParseStatus_e tlvStatus;
    TlvParsingStatusExtIf_e tlvStatus;

    
    if( cfgFile != NULL)
    {
      cfgFileName = cfgFile;
      printf("Got CfgFile \"%s\"", cfgFileName);
    }
    else
    {
       goto gimReply;
    }

    char cmd[80];
    sprintf(cmd, "sysevent set docsis_cfg_file %s", cfgFileName);
    system(cmd);

    printf("sysevent set docsis_cfg_file %s\n", cfgFileName);

    if (stat(cfgFileName, &cfgFileStat) != 0)
    {
        printf("Cannot stat eSafe Config file \"%s\", %s, aborting Config file", cfgFileName, strerror(errno));
        goto gimReply;
    }
    cfgFileBuffLen = cfgFileStat.st_size;
    if (cfgFileBuffLen == 0)
    {
        /* No eSafe TLVs --> No eRouter TLVs */
        printf("CfgFile \"%s\" is empty", cfgFileName);
        goto gimReply;
    }

    cfgFileBuff = malloc(cfgFileBuffLen);
    if (cfgFileBuff == NULL)
    {
        printf("Cannot alloc buffer for eSafe Config file \"%s\", aborting Config file", cfgFileName, strerror(errno));
        goto gimReply;
    }

    if ((cfgFd = open(cfgFileName, O_RDONLY)) < 0)
    {
        printf("Cannot open eSafe Config file \"%s\", %s, aborting Config file", cfgFileName, strerror(errno));
        goto freeMem;
    }

    if ((actualNumBytes = read(cfgFd, cfgFileBuff, cfgFileBuffLen)) < 0)
    {
        printf("Cannot read eSafe Config file \"%s\", %s, aborting Config file", cfgFileName, strerror(errno));
        goto closeFile;
    }
    else if (actualNumBytes != cfgFileBuffLen)
    {
        printf("eSafe Config file \"%s\", actual len (%d) different than stat (%d), aborting Config file", cfgFileName, actualNumBytes, cfgFileBuffLen);
        goto closeFile;
    }

    oldRouterMode = eRouterMode;

    
    tlvStatus = parseTlv(cfgFileBuff, cfgFileBuffLen);

    if (tlvStatus != TLV_OK_extIf)
    {
        printf("eSafe Config file \"%s\", parsing error (%d), aborting Config file", cfgFileName, tlvStatus);
        goto closeFile;
    }

    printf("eSafe Config file \"%s\", parsed completed, status %d\n", cfgFileName, tlvStatus);

    GWP_UpdateERouterMode();

closeFile:
    /* Close file */
    if (cfgFd >= 0)
    {
        close(cfgFd);
    }
freeMem:
    /* Free memory */
    if (cfgFileBuff != NULL)
    {
        free(cfgFileBuff);
    }
gimReply:

    /* Reply to GIM SRN */
    notificationReply_CfgFileForEsafe();
    

    return 0;
}

/**************************************************************************/
/*! \fn int GWP_act_StartActiveUnprovisioned(SME_APP_T *app, SME_EVENT_T *event);
 **************************************************************************
 *  \brief Actions for starting active gw, before DOCSIS cfg file
 *  \param[in] SME Handler params
 *  \return 0
**************************************************************************/
//static int GWP_act_StartActiveUnprovisioned(SME_APP_T *app, SME_EVENT_T *event)
static int GWP_act_StartActiveUnprovisioned()
{
    Char *cmdline;

    /* Update esafe db with router provisioning status*/
    
    eSafeDevice_SetProvisioningStatusProgress(ESAFE_PROV_STATE_IN_PROGRESS_extIf);
	
    printf("Starting ActiveUnprovisioned processes");

    /* Add paths for eRouter dev counters */
    printf("Adding PP paths");
    cmdline = "add "IFNAME_ETH_0" cni0 " ER_NETDEVNAME " in";
    COMMONUTILS_file_write("/proc/net/ti_pp_path", cmdline, strlen(cmdline));
    cmdline = "add cni0 "IFNAME_ETH_0" " ER_NETDEVNAME " out";
    COMMONUTILS_file_write("/proc/net/ti_pp_path", cmdline, strlen(cmdline));

    /*printf("Starting COSA services\n");
    system("sh /etc/utopia/service.d/service_cosa.sh cosa-start");*/
    
    /* Start webgui in PCD after P&M is fully initialized */
    /*
    printf("Starting WebGUI\n");
    system("sh /etc/webgui.sh");
    */
    return 0;
}

/**************************************************************************/
/*! \fn int GWP_act_InactiveBefCfgfile(SME_APP_T *app, SME_EVENT_T *event);
 **************************************************************************
 *  \brief Actions for inactive gw, before DOCSIS cfg file
 *  \param[in] SME Handler params
 *  \return 0
**************************************************************************/
//static int GWP_act_InactiveBefCfgfile(SME_APP_T *app, SME_EVENT_T *event)
static int GWP_act_InactiveBefCfgfile()
{
    /* Update esafe db with router provisioning status*/
    
    eSafeDevice_SetProvisioningStatusProgress(ESAFE_PROV_STATE_NOT_INITIATED_extIf);

    printf("******************************");
    printf("* Disabled (before cfg file) *");
    printf("******************************");

    /*printf("Starting forwarding service\n");
    system("sysevent set forwarding-start");*/

    /*printf("Starting COSA services\n");
    system("sh /etc/utopia/service.d/service_cosa.sh cosa-start");*/

    /*printf("Starting WebGUI\n");
    system("sh /etc/webgui.sh");*/

    return 0;
}

/**************************************************************************/
/*! \fn int GWP_act_BefCfgfileEntry(SME_APP_T *app, SME_EVENT_T *event);
 **************************************************************************
 *  \brief Actions at entry to BefCfgfile
 *  \param[in] SME Handler params
 *  \return 0
**************************************************************************/
//static int GWP_act_BefCfgfileEntry_callback(SME_APP_T *app, SME_EVENT_T *event)
static int GWP_act_BefCfgfileEntry_callback()
{
    if (GWP_IsGwEnabled())
    {
        
        return GWP_act_StartActiveUnprovisioned();
    }
    else
    {
        
        return GWP_act_InactiveBefCfgfile();
    }
}

/**************************************************************************/
/*! \fn int GWP_act_DocsisInited(SME_APP_T *app, SME_EVENT_T *event);
 **************************************************************************
 *  \brief Actions when DOCSIS is initialized
 *  \param[in] SME Handler params
 *  \return 0
**************************************************************************/
static int GWP_act_DocsisInited_callback()
{
    esafeErouterOperModeExtIf_e operMode;
    char macstr[20];
    Uint8 lladdr[ NETUTILS_IPv6_GLOBAL_ADDR_LEN / sizeof(Uint8) ];
    Uint8 soladdr[ NETUTILS_IPv6_GLOBAL_ADDR_LEN / sizeof(Uint8) ];
    char soladdrKey[64];
    char soladdrStr[64];

    /* Docsis initialized */
    printf("Got DOCSIS Initialized");

    // printf("Utopia init done\n");
    printf("Loading erouter0 network interface driver\n");
    system("insmod " ERNETDEV_MODULE " netdevname=" ER_NETDEVNAME);

    {
        macaddr_t macAddr;
        
        getWanMacAddress(&macAddr);
       
       setNetworkDeviceMacAddress(ER_NETDEVNAME,&macAddr);
    }  

    
    getDocsisDbFactoryMode(&factory_mode);
    

    if (factory_mode) {
        //GWP_SysCfgSetInt("bridge_mode", 2);
        GWP_SysCfgSetInt("mgmt_lan_telnetaccess", 1);
        //GWP_SysCfgSetInt("last_erouter_mode", 0);
     }

    bridge_mode = GWP_SysCfgGetInt("bridge_mode");
    eRouterMode = GWP_SysCfgGetInt("last_erouter_mode");
    if (bridge_mode == 0)
    {
        
        bridge_mode = eRouterMode == DOCESAFE_ENABLE_DISABLE_extIf ? 2 : 0;
    }

    char sysevent_cmd[80];
    snprintf(sysevent_cmd, sizeof(sysevent_cmd), "sysevent set bridge_mode %d", bridge_mode);
    system(sysevent_cmd);

    GWP_DocsisInited();

    system("sysevent set docsis-initialized 1");

    /* Must set the ESAFE Enable state before replying to the DocsisInit event */
    GWP_UpdateEsafeAdminMode(eRouterMode);

    /* Set operMode */
    //if (eRouterMode == DOCESAFE_ENABLE_DISABLE)
    if (eRouterMode == DOCESAFE_ENABLE_DISABLE_extIf)
    {
        /* Disabled */
        operMode = DOCESAFE_EROUTER_OPER_DISABLED_extIf;
    }
    else
    {
        /* At this point: enabled, but neither are provisioned (regardless of which is enabled) */
        operMode = DOCESAFE_EROUTER_OPER_NOIPV4_NOIPV6_extIf;
    }
    
    eSafeDevice_SetErouterOperationMode(operMode);

  
   	eSafeDevice_SetServiceIntImpact();

    /* Disconnect docsis LB */
    printf("Disconnecting DOCSIS local bridge");
   
    connectLocalBridge(False);

    /* This is an SRN, reply */
    printf("Got Docsis INIT - replying");
   
    notifyDocsisInitializedResponse();

    sysevent_fd = sysevent_open("127.0.0.1", SE_SERVER_WELL_KNOWN_PORT, SE_VERSION, "gw_prov", &sysevent_token);

    if (sysevent_fd >= 0)
    {
        system("sysevent set phylink_wan_state down");
        pthread_create(&sysevent_tid, NULL, GWP_sysevent_threadfunc, NULL);
    }
    
    //calcualte erouter base solicited node address
   
    getInterfaceLinkLocalAddress(ER_NETDEVNAME, lladdr);
    
    getMultiCastGroupAddress(lladdr,soladdr);
  	
    snprintf(soladdrKey, sizeof(soladdrKey), "ipv6_%s_ll_solicNodeAddr", ER_NETDEVNAME);
    inet_ntop(AF_INET6, soladdr, soladdrStr, sizeof(soladdrStr));
    sysevent_set(sysevent_fd, sysevent_token, soladdrKey, soladdrStr,0);
    
    //calculate cm base solicited node address
    
    getInterfaceLinkLocalAddress(IFNAME_WAN_0, lladdr);
    
   
    getMultiCastGroupAddress(lladdr,soladdr);
  	
    snprintf(soladdrKey, sizeof(soladdrKey), "ipv6_%s_ll_solicNodeAddr", IFNAME_WAN_0);
    inet_ntop(AF_INET6, soladdr, soladdrStr, sizeof(soladdrStr));
    sysevent_set(sysevent_fd, sysevent_token, soladdrKey, soladdrStr,0);
    
    //StartDocsis();

    return 0;
}


/**************************************************************************/
/*! \fn int DCR_act_ProvEntry(SME_APP_T *app, SME_EVENT_T *event);
 **************************************************************************
 *  \brief Actions at entry to gw provisioning
 *  \param[in] SME Handler params
 *  \return 0
**************************************************************************/
static int GWP_act_ProvEntry_callback()
{
    int i;
    
    //system("sysevent set lan-start");
   
/* TODO: OEM to implement swctl apis */

    /* Register on docsis Init event */
   
    registerDocsisInitEvents(); 
    system("/etc/utopia/utopia_init.sh");

    syscfg_init();

    

    printf("Waiting for Docsis INIT");

    /* Now that we have the ICC que (SME) and we are registered on the docsis INIT    */
    /* event, we can notify PCD to continue                                           */
    sendProcessReadySignal();

    /* Initialize Switch */
    // VEN_SWT_InitSwitch();
    

    return 0;
}

static int GWP_act_DocsisTftpOk_callback(){
    gDocTftpOk = 1;
    if(snmp_inited) {
        
         if(startDocsisCfgParsing() != STATUS_OK) {
            printf("fail to start docsis CFG parsing!!\n");
        }
    }
    return 0;
}

// static int get_ipv6_addrs() {
//     
// }

// static int GWP_act_DocsisDHCPv6Bind(SME_APP_T *app, SME_EVENT_T *event){
//     
// }

/*static void StartDocsis() {
    if(DocsisIf_StartDocsisManager() != STATUS_OK)
    {
       LOG_GW_ERROR("fail to start docsis!!\n");
    }
    return;
}*/

static void LAN_start() {
    int i;
    char buf[10];
    
    if (bridge_mode == 0)
    {
        printf("Utopia starting lan...\n");
        sysevent_set(sysevent_fd, sysevent_token, "lan-start", "", 0);
        
        
    } else {
        // TODO: fix this
        printf("Utopia starting bridge...\n");
        sysevent_set(sysevent_fd, sysevent_token, "bridge-start", "", 0);
    }
    
    //ADD MORE LAN NETWORKS HERE
    
    sysevent_set(sysevent_fd, sysevent_token, "dhcp_server-resync", "", 0);
   
	/* TODO: OEM to implement swctl apis */

    if(gDocTftpOk) {
        if(startDocsisCfgParsing() != STATUS_OK) {
            printf("fail to start docsis CFG parsing!!\n");
        }
    }
    return;
}

/**************************************************************************/
/*! \fn int main(int argc, char *argv)
 **************************************************************************
 *  \brief Init and run the Provisioning process
 *  \param[in] argc
 *  \param[in] argv
 *  \return Currently, never exits
 **************************************************************************/
int main(int argc, char *argv[])
{
    printf("Started gw_prov_utopia");
   
    if( findProcessId(argv[0]) > 0 )
    {
        printf("Already running");
        return 1;
    }

    printf("Register exception handlers");
    
    registerProcessExceptionHandlers(argv[0]);

    GWP_InitDB();

    appCallBack *obj = NULL;
    obj = (appCallBack*)malloc(sizeof(appCallBack));
	
    obj->pGWP_act_DocsisLinkDown_1 =  GWP_act_DocsisLinkDown_callback_1;
    obj->pGWP_act_DocsisLinkDown_2 =  GWP_act_DocsisLinkDown_callback_2;
    obj->pGWP_act_DocsisLinkUp = GWP_act_DocsisLinkUp_callback;
    obj->pGWP_act_DocsisCfgfile = GWP_act_DocsisCfgfile_callback;
    obj->pGWP_act_DocsisTftpOk = GWP_act_DocsisTftpOk_callback;
    obj->pGWP_act_BefCfgfileEntry = GWP_act_BefCfgfileEntry_callback;
    obj->pGWP_act_DocsisInited = GWP_act_DocsisInited_callback;
    obj->pGWP_act_ProvEntry = GWP_act_ProvEntry_callback;
    obj->pDocsis_gotEnable = docsis_gotEnable_callback;
    obj->pGW_Tr069PaSubTLVParse = GW_Tr069PaSubTLVParse;
    	
    /* Command line - ignored */
    SME_CreateEventHandler(obj);
    

    return 0;
}


