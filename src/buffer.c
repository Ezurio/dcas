#define _BSD_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#include "debug.h"
#include "sdc_sdk.h"
#include "buffer.h"
#include "dcal_api.h"
#include "version.h"

#include "dcal_builder.h"
#include "dcal_verifier.h"
#undef ns
#define ns(x) FLATBUFFERS_WRAP_NAMESPACE(DCAL_session, x)
#include "flatcc/support/hexdump.h"

#define LAIRD_HELLO "HELLO DCAS"
#define LAIRD_RESPONSE "WELCOME TO FAIRFIELD"
#define LAIRD_BAD_BUFFER "BAD FLAT BUFFER"

#define SDKLOCK(x) (pthread_mutex_lock(x))
#define SDKUNLOCK(x) (pthread_mutex_unlock(x))

// a 0 return code means invalid buffer
flatbuffers_thash_t verify_buffer(const void * buf, const size_t size)
{
	flatbuffers_thash_t ret;
	if ((buf==NULL) || (size==0))
		return 0;

	ret = flatbuffers_get_type_hash(buf);
	switch(ret) {
		case ns(Handshake_type_hash):
			if(ns(Handshake_verify_as_root(buf,size))){
				DBGERROR("%s: unable to verify buffer\n", __func__);
				ret = 0;
				}
			break;
		case ns(Status_type_hash):
			if(ns(Status_verify_as_root(buf,size))){
				DBGERROR("%s: unable to verify buffer\n", __func__);
				ret = 0;
				}
			break;
		case ns(Command_type_hash):
			if(ns(Command_verify_as_root(buf,size))){
				DBGERROR("%s: unable to verify buffer\n", __func__);
				ret = 0;
				}
			break;
		case ns(U32_type_hash):
			if(ns(U32_verify_as_root(buf,size))){
				DBGERROR("%s: unable to verify buffer\n", __func__);
				ret = 0;
				}
			break;
		case ns(Version_type_hash):
			if(ns(Version_verify_as_root(buf,size))){
				DBGERROR("%s: unable to verify buffer\n", __func__);
				ret = 0;
				}
			break;
		case ns(Globals_type_hash):
			if(ns(Globals_verify_as_root(buf,size))){
				DBGERROR("%s: unable to verify buffer\n", __func__);
				ret = 0;
				}
			break;
		case ns(Profile_type_hash):
			if(ns(Profile_verify_as_root(buf,size))){
				DBGERROR("%s: unable to verify buffer\n", __func__);
				ret = 0;
				}
			break;
		case ns(P_entry_type_hash):
			if(ns(P_entry_verify_as_root(buf,size))){
				DBGERROR("%s: unable to verify buffer\n", __func__);
				ret = 0;
				}
			break;
		case ns(Profile_list_type_hash):
			if(ns(Profile_verify_as_root(buf,size))){
				DBGERROR("%s: unable to verify buffer\n", __func__);
				ret = 0;
				}
			break;
		case ns(Time_type_hash):
			if(ns(Time_verify_as_root(buf,size))){
				DBGERROR("%s: unable to verify buffer\n", __func__);
				ret = 0;
				}
			break;
		default:
			DBGERROR("%s: buffer hash invalid: %lx\n", __func__, (unsigned long)ret);
			ret = 0;
	}
	return ret;
}

const char * buftype_to_string(flatbuffers_thash_t buftype)
{
	switch(buftype) {
		case ns(Handshake_type_hash):
			return "Handshake";
			break;
		case ns(Status_type_hash):
			return "Status";
			break;
		case ns(Command_type_hash):
			return "Command";
			break;
		case ns(U32_type_hash):
			return "U32";
			break;
		case ns(Version_type_hash):
			return "Version";
			break;
		case ns(Globals_type_hash):
			return "Globals";
			break;
		case ns(Profile_type_hash):
			return "Profile";
			break;
		case ns(P_entry_type_hash):
			return "Profile list entry";
			break;
		case ns(Profile_list_type_hash):
			return "Profile list";
			break;
		case ns(Time_type_hash):
			return "Time";
			break;

		default:
			return("unrecognized\n");
	}
}

int is_handshake_valid( ns(Handshake_table_t) handshake)
{
	#ifdef DEBUG_BUILD
	const char * ip;
	#endif
	int ret;

	if (ns(Handshake_server(handshake)) == true) {
		DBGERROR("Handshake marked as from server\n");
		return 0;
	}

	#ifdef DEBUG_BUILD
	ip = ns(Handshake_ip(handshake));
	#endif
	DBGINFO("Handshake ip: %s\n", ip);

	if (ns(Handshake_magic(handshake)) == ns(Magic_HELLO))
		return 1;

	return 0;
}

int build_handshake_ack(flatcc_builder_t *B, unsigned int error)
{
	flatcc_builder_reset(B);
	flatbuffers_buffer_start(B, ns(Handshake_type_identifier));
	ns(Handshake_start(B));
	ns(Handshake_server_add(B, true));

	if (error)
		ns(Handshake_magic_add(B, ns(Magic_NACK)));
	else
		ns(Handshake_magic_add(B, ns(Magic_ACK)));

	//TODO - do we want our ip address in the handshake from server?  If so
	//we need to get from the ssh session somehow so we know what interface
	//Could have it included by default in process_buffer call
//	ns(Handshake_ip_create_str(B, "192.168.0.1"));
	ns(Handshake_api_level_add(B, DCAL_VERSION));
	ns(Handshake_error_add(B, error));
	ns(Handshake_end_as_root(B));

	return 0;
}

//return codes:
//0 - success
//positive value - benign error
//negative value - unrecoverable error
int build_status(flatcc_builder_t *B, pthread_mutex_t *sdk_lock)
{
	CF10G_STATUS status = {0};

	status.cardState = CARDSTATE_AUTHENTICATED;
	SDCERR result;
	LRD_WF_SSID ssid = {0};
	LRD_WF_ipv6names *ipv6_names = NULL;
	size_t num_ips = 0;

	SDKLOCK(sdk_lock);
	result = GetCurrentStatus(&status);
	SDKUNLOCK(sdk_lock);
	if (result!=SDCERR_SUCCESS){
		DBGERROR("GetCurrentStatus() failed with %d\n", result);
		return result;
	}
	SDKLOCK(sdk_lock);
	result = LRD_WF_GetSSID(&ssid);
	SDKUNLOCK(sdk_lock);
	if (result!=SDCERR_SUCCESS){
		// there are conditions such as disabled where this could fail
		// and we don't want to abort sending back status, so no return
		// here - just log it.
		DBGINFO("LRD_WF_GetSSID() failed with %d\n", result);
	}

// only dealing with client mode for now
	flatcc_builder_reset(B);
	flatbuffers_buffer_start(B, ns(Status_type_identifier));
	ns(Status_start(B));
	ns(Status_cardState_add(B, status.cardState));
	ns(Status_ProfileName_create_str(B, status.configName));
	if (ssid.len > LRD_WF_MAX_SSID_LEN)
		ssid.len = LRD_WF_MAX_SSID_LEN;  // should never happen
	ns(Status_ssid_create(B, (unsigned char *)ssid.val, ssid.len));
	ns(Status_channel_add(B, status.channel));
	ns(Status_rssi_add(B, status.rssi));
	ns(Status_clientName_create_str(B, status.clientName));
	ns(Status_mac_create(B, (unsigned char *)status.client_MAC, MAC_SZ));
	ns(Status_ip_create(B, (unsigned char *)status.client_IP, IP4_SZ));
	ns(Status_AP_mac_create(B, (unsigned char *)status.AP_MAC, MAC_SZ));
	ns(Status_AP_ip_create(B, (unsigned char *)status.AP_IP, IP4_SZ));
	ns(Status_AP_name_create_str(B, status.APName));
	ns(Status_bitRate_add(B, status.bitRate));
	ns(Status_txPower_add(B, status.txPower));
	ns(Status_dtim_add(B, status.DTIM));
	ns(Status_beaconPeriod_add(B, status.beaconPeriod));

	SDKLOCK(sdk_lock);
	result = LRD_WF_GetIpV6Address(NULL, &num_ips);
	ipv6_names = (LRD_WF_ipv6names*)malloc(sizeof(LRD_WF_ipv6names)*(num_ips+3));
	if (ipv6_names==NULL){
		SDKUNLOCK(sdk_lock);
		return SDCERR_INSUFFICIENT_MEMORY;
	}
	result = LRD_WF_GetIpV6Address(ipv6_names, &num_ips);
	SDKUNLOCK(sdk_lock);
	if(result!=SDCERR_SUCCESS){
		free(ipv6_names);
		return result;
	}
	flatbuffers_string_vec_ref_t flatc_ipnames[num_ips];

	for (size_t i=0; i< num_ips; i++)
		flatc_ipnames[i]=flatbuffers_string_create_str(B, ipv6_names[i]);
	flatbuffers_string_vec_ref_t fcv_addresses = flatbuffers_string_vec_create(B, flatc_ipnames, num_ips);

	ns(Status_ipv6_add(B, fcv_addresses));

	ns(Status_end_as_root(B));

	free(ipv6_names);
	return 0;
}

//return codes:
//0 - success
//positive value - benign error
//negative value - unrecoverable error
int build_version(flatcc_builder_t *B, pthread_mutex_t *sdk_lock)
{
	SDCERR result;
	CF10G_STATUS status = {0};
	unsigned long longsdk = 0;
	int size = STR_SZ;
	RADIOCHIPSET chipset;
	LRD_SYSTEM sys;
	int sdk;
	unsigned int driver;
	unsigned int dcas;
	unsigned int dcal;
	char firmware[STR_SZ];
	char supplicant[STR_SZ];
	char release[STR_SZ];

	inline void remove_cr(char * str)
	{
		int i;
		for (i=0; i<STR_SZ; i++)
			if (str[i]==0xa)
				str[i]=0;
	}

	SDKLOCK(sdk_lock);
	result = GetCurrentStatus(&status);
	if (result == SDCERR_SUCCESS)
		result = GetSDKVersion(&longsdk);
	if (result == SDCERR_SUCCESS)
		result = LRD_WF_GetRadioChipSet(&chipset);
	if (result == SDCERR_SUCCESS)
		result = LRD_WF_System_ID(&sys);
	if (result == SDCERR_SUCCESS)
		result = LRD_WF_GetFirmwareVersionString(firmware, &size);
	SDKUNLOCK(sdk_lock);
	if (result)
		return result;

	sdk = longsdk;
	dcas = DCAS_COMPONENT_VERSION;
	driver = status.driverVersion;

	FILE *in = popen( "sdcsupp -qv", "r");
	if (in){
		fgets(supplicant, STR_SZ, in);
		supplicant[STR_SZ-1]=0;
		pclose(in);
	} else
		strcpy(supplicant, "none");

	int sysfile = open ("/etc/laird-release", O_RDONLY);
	if ((sysfile==-1) && (errno==ENOENT))
		sysfile = open ("/etc/summit-release", O_RDONLY);
	if (sysfile > 1){
		read(sysfile, release, STR_SZ);
		release[STR_SZ-1]=0;
		close(sysfile);
	}else
		strcpy(release, "unknown");

/// have various versions - now build buffer
	remove_cr(supplicant);
	remove_cr(release);

	flatcc_builder_reset(B);
	flatbuffers_buffer_start(B, ns(Version_type_identifier));
	ns(Version_start(B));
	ns(Version_sdk_add(B, sdk));
	ns(Version_chipset_add(B, chipset));
	ns(Version_sys_add(B, sys));
	ns(Version_driver_add(B, driver));
	ns(Version_dcas_add(B, dcas));
	ns(Version_firmware_create_str(B, firmware));
	ns(Version_supplicant_create_str(B, supplicant));
	ns(Version_release_create_str(B, release));

	ns(Version_end_as_root(B));

	return 0;
}

//return codes:
//0 - success
//positive value - benign error
//negative value - unrecoverable error
int do_enable_disable(flatcc_builder_t *B, pthread_mutex_t *sdk_lock, bool enable)
{
	SDCERR result;
	SDKLOCK(sdk_lock);
		if (enable)
			result = RadioEnable();
		else
			result = RadioDisable();
	SDKUNLOCK(sdk_lock);

	if (result != SDCERR_SUCCESS)
		return result;

	build_handshake_ack(B, 0);
	return 0;
}

#define user(p) (char*)ns(Profile_security1(p))
#define password(p) (char*)ns(Profile_security2(p))
#define psk(p) (char*)ns(Profile_security1(p))
#define cacert(p) (char*)ns(Profile_security3(p))
#define pacfilename(p) (char*)ns(Profile_security3(p))
#define pacpassword(p) (char*)ns(Profile_security4(p))
#define usercert(p) (char*)ns(Profile_security4(p))
#define usercertpassword(p) (char*)ns(Profile_security5(p))

#define weplen(s) ((strlen(s)==5)?WEPLEN_40BIT:(strlen(s)==16)?WEPLEN_128BIT:WEPLEN_NOT_SET)

SDCERR LRD_WF_AutoProfileCfgControl(const char *name, unsigned char enable);
SDCERR LRD_WF_AutoProfileCfgStatus(const char *name, unsigned char *enabled);
SDCERR LRD_WF_AutoProfileControl(unsigned char enable);
SDCERR LRD_WF_AutoProfileStatus(unsigned char *enable);

//return codes:
//0 - success
//positive value - benign error
//negative value - unrecoverable error
int do_set_profile(flatcc_builder_t *B, ns(Command_table_t) cmd, pthread_mutex_t *sdk_lock)
{
	ns(Profile_table_t) profile;
	SDCConfig config = {{0}};
	int ret;

	//TODO we ought to do some assertion that the cmd_table is a profile
	profile = ns(Command_cmd_pl(cmd));

	strncpy(config.configName, ns(Profile_name(profile)), CONFIG_NAME_SZ);
	assert(flatbuffers_uint8_vec_len(ns(Profile_ssid(profile))) <= SSID_SZ);

	memcpy(&config.SSID, ns(Profile_ssid(profile)), flatbuffers_uint8_vec_len(ns(Profile_ssid(profile))));

	strncpy(config.clientName, ns(Profile_client_name(profile)), CLIENT_NAME_SZ);

	config.txPower = ns(Profile_txPwr(profile));
	config.authType = ns(Profile_auth(profile));
	config.eapType = ns(Profile_eap(profile));
	config.powerSave = ns(Profile_pwrsave(profile));
	config.powerSave |= (ns(Profile_pspDelay(profile)) << 16);
	config.wepType = ns(Profile_weptype(profile));
	config.bitRate = ns(Profile_bitrate(profile));
	config.radioMode = ns(Profile_radiomode(profile));

	switch(config.wepType) {
		case WEP_ON:
		case WEP_AUTO:
			ret = SetMultipleWEPKeys( &config, ns(Profile_weptxkey(profile)),
			                weplen(ns(Profile_security1(profile))),
			                (unsigned char*)ns(Profile_security1(profile)),
			                weplen(ns(Profile_security2(profile))),
			                (unsigned char*)ns(Profile_security2(profile)),
			                weplen(ns(Profile_security3(profile))),
			                (unsigned char*)ns(Profile_security3(profile)),
			                weplen(ns(Profile_security4(profile))),
			                (unsigned char*)ns(Profile_security4(profile)));
			break;

		case WPA_PSK:
		case WPA2_PSK:
		case WPA_PSK_AES:
		case WAPI_PSK:
			SetPSK(&config, (char*) psk(profile));
			break;

		case WEP_OFF:
			// dont set any security elements
			break;
		default:
			switch(config.eapType){
				case EAP_LEAP:
					SetLEAPCred(&config, user(profile), password(profile));
					break;
				case EAP_EAPTTLS:
					SetEAPTTLSCred(&config, user(profile), password(profile),
					                CERT_FILE, cacert(profile));
					break;
				case EAP_PEAPMSCHAP:
					SetPEAPMSCHAPCred(&config, user(profile), password(profile),
					               CERT_FILE, cacert(profile));
					break;
				case EAP_PEAPGTC:
					SetPEAPGTCCred(&config, user(profile), password(profile),
					               CERT_FILE, cacert(profile));
					break;
				case EAP_EAPFAST:
					SetEAPFASTCred(&config, user(profile), password(profile),
					               pacfilename(profile), pacpassword(profile));
					break;
				case EAP_EAPTLS:
					SetEAPTLSCred(&config, user(profile), usercert(profile),
					              CERT_FILE, cacert(profile));
					break;
				case EAP_PEAPTLS:
					SetPEAPTLSCred(&config, user(profile), usercert(profile),
					              CERT_FILE, cacert(profile));
					break;
				case EAP_NONE:
				case EAP_WAPI_CERT:
				default:
					// do nothing
					break;
			}
			break;
	}

	SDKLOCK(sdk_lock);
	ret = AddConfig(&config);
	if (ret==SDCERR_INVALID_NAME)
		ret = ModifyConfig(config.configName, &config);

	LRD_WF_AutoProfileCfgControl(config.configName, ns(Profile_autoprofile(profile)));
	SDKUNLOCK(sdk_lock);
	build_handshake_ack(B, ret);
	return ret;
}

//return codes:
//0 - success
//positive value - benign error
//negative value - unrecoverable error
int do_get_profile(flatcc_builder_t *B, ns(Command_table_t) cmd, pthread_mutex_t *sdk_lock)
{
	ns(String_table_t) profile_name;
	SDCConfig config = {{0}};
	int ret;
	unsigned char apStatus = 0;

	//TODO we ought to do some assertion that the cmd_table is a string
	profile_name = ns(Command_cmd_pl(cmd));

	ret = GetConfig((char*) ns(String_value(profile_name)), &config);
	
	if (ret)
		build_handshake_ack(B, ret);
	else
	{
		flatcc_builder_reset(B);
		flatbuffers_buffer_start(B, ns(Profile_type_identifier));
		ns(Profile_start(B));

		ns(Profile_name_create_str(B, config.configName));
		ns(Profile_ssid_create(B, (unsigned char *)config.SSID, strlen(config.SSID)));
		ns(Profile_client_name_create_str(B, config.clientName));
		ns(Profile_txPwr_add(B, config.txPower));
		ns(Profile_pwrsave_add(B, config.powerSave&0xffff));
		ns(Profile_pspDelay_add(B, (config.powerSave >>16)&0xffff));
		ns(Profile_weptype_add(B, config.wepType));
		ns(Profile_auth_add(B, config.authType));
		ns(Profile_eap_add(B, config.eapType));
		ns(Profile_bitrate_add(B, config.bitRate));
		ns(Profile_radiomode_add(B, config.radioMode));
		SDKLOCK(sdk_lock);
		LRD_WF_AutoProfileCfgStatus((char*) ns(String_value(profile_name)), &apStatus);
		SDKUNLOCK(sdk_lock);
		ns(Profile_autoprofile_add(B, apStatus));

		switch(config.wepType){
			case WEP_ON:
			case WEP_AUTO:
			case WEP_CKIP:
			case WEP_AUTO_CKIP:
			{
				unsigned char key[4][26];
				WEPLEN klen[4];
				int txkey;
				GetMultipleWEPKeys(&config, &txkey, &klen[0], key[0],
					&klen[1], key[1], &klen[2], key[2], &klen[3], key[3]);

				if(klen[0])
					ns(Profile_security1_create_str(B, "1"));
				if(klen[1])
					ns(Profile_security2_create_str(B, "1"));
				if(klen[2])
					ns(Profile_security3_create_str(B, "1"));
				if(klen[3])
					ns(Profile_security4_create_str(B, "1"));
				ns(Profile_weptxkey_add(B, txkey));}
			break;
			case WPA_PSK:
			case WPA2_PSK:
			case WPA_PSK_AES:
			case WPA2_PSK_TKIP:
			case WAPI_PSK:{
				char psk[PSK_SZ] = {0};
				GetPSK(&config, psk);

				if (strlen(psk))
					ns(Profile_security1_create_str(B, "1"));
			}
			break;
			default: { // EAPs
				char user[USER_NAME_SZ] = {0};
				char pw[USER_PWD_SZ] = {0};
				char usercert[CRED_CERT_SZ] = {0};
				char cacert[CRED_CERT_SZ] = {0};
				char pacfn[CRED_PFILE_SZ] = {0};
				char pacpw[CRED_PFILE_SZ] = {0};
				char usercrtpw[USER_PWD_SZ] = {0};

				switch (config.eapType) {
					case EAP_EAPFAST:
						GetEAPFASTCred(&config, user, pw, pacfn, pacpw);
					break;
					case EAP_PEAPMSCHAP:
						GetPEAPMSCHAPCred(&config, user, pw, NULL, cacert);
					break;
					case EAP_EAPTLS:
						GetEAPTLSCred(&config, user, usercert, NULL, cacert);
					break;
					case EAP_EAPTTLS:
						GetEAPTTLSCred(&config, user, usercert, NULL, cacert);
						GetUserCertPassword(&config, usercrtpw);
					break;
					case EAP_PEAPTLS:
						GetPEAPTLSCred(&config, user, usercert, NULL, cacert);
						GetUserCertPassword(&config, usercrtpw);
					break;
					case EAP_LEAP:
						GetLEAPCred(&config, user, pw);
					break;
					case EAP_PEAPGTC:
						GetPEAPGTCCred(&config, user, pw, NULL, cacert);
					break;
					default:
					// noop
					break;
				}

				if (strlen(user))
					ns(Profile_security1_create_str(B, "1"));

				if (strlen(pw))
					ns(Profile_security2_create_str(B, "1"));

				if (strlen(cacert) || strlen(pacfn))
					ns(Profile_security3_create_str(B, "1"));

				if (strlen(pacpw) || strlen(usercert))
					ns(Profile_security4_create_str(B, "1"));

				if (strlen(usercrtpw))
					ns(Profile_security5_create_str(B, "1"));
			}
			break;
	}

	ns(Profile_end_as_root(B));
	}
	return 0;
}

//return codes:
//0 - success
//positive value - benign error
//negative value - unrecoverable error
int do_del_profile(flatcc_builder_t *B, ns(Command_table_t) cmd, pthread_mutex_t *sdk_lock)
{
	ns(String_table_t) profile_name;
	int ret;

	//TODO we ought to do some assertion that the cmd_table is a string
	profile_name = ns(Command_cmd_pl(cmd));

	ret = DeleteConfig((char*) ns(String_value(profile_name)));
	
	build_handshake_ack(B, ret);

	return 0; // any error is already in ack/Nack
}

//return codes:
//0 - success
//positive value - benign error
//negative value - unrecoverable error
int do_get_profile_list(flatcc_builder_t *B, pthread_mutex_t *sdk_lock)
{
	return SDCERR_NOT_IMPLEMENTED;
}

//return codes:
//0 - success
//positive value - benign error
//negative value - unrecoverable error
int do_activate_profile(flatcc_builder_t *B, ns(Command_table_t) cmd, pthread_mutex_t *sdk_lock)
{
	ns(String_table_t) string;
	int ret;
	string = ns(Command_cmd_pl(cmd));

	SDKLOCK(sdk_lock);
	ret = ActivateConfig((char*)ns(String_value(string)));
	SDKUNLOCK(sdk_lock);
	build_handshake_ack(B, ret);

	return ret;
}

//return codes:
//0 - success
//positive value - benign error
//negative value - unrecoverable error
int do_issue_radiorestart(flatcc_builder_t *B, pthread_mutex_t * sdk_lock)
{
	int ret;
	ret = system("ifrc wlan0 restart");
	build_handshake_ack(B, ret);
	return ret;
}

//return codes:
//0 - success
//positive value - benign error
//negative value - unrecoverable error
SDCERR setIgnoreNullSsid(unsigned long value);
SDCERR getIgnoreNullSsid(unsigned long *value);
int do_get_globals(flatcc_builder_t *B, pthread_mutex_t *sdk_lock)
{
	SDCGlobalConfig gcfg = {0};
	int ret;
	unsigned long ignoreNullSSID = 0;
	unsigned char apStatus = 0;

	ret = GetGlobalSettings(&gcfg);
	
	if (ret)
		build_handshake_ack(B, ret);
	else
	{
		flatcc_builder_reset(B);
		flatbuffers_buffer_start(B, ns(Globals_type_identifier));
		ns(Globals_start(B));

		SDKLOCK(sdk_lock);
		getIgnoreNullSsid(&ignoreNullSSID);
		LRD_WF_AutoProfileStatus(&apStatus);
		SDKUNLOCK(sdk_lock);

	//	ns(Profile_security5_create_str(B, "1"));
		ns(Globals_auth_add(B, gcfg.authServerType));
		ns(Globals_channel_set_a_add(B, gcfg.aLRS));
		ns(Globals_channel_set_b_add(B, gcfg.bLRS));
		ns(Globals_auto_profile_add(B, apStatus));
		ns(Globals_beacon_miss_add(B, gcfg.BeaconMissTimeout));
		ns(Globals_bt_coex_add(B, gcfg.BTcoexist));
		ns(Globals_ccx_add(B, gcfg.CCXfeatures));
		ns(Globals_cert_path_create_str(B, gcfg.certPath));
		ns(Globals_date_check_add(B,(gcfg.suppInfo & SUPPINFO_TLS_TIME_CHECK)));
		ns(Globals_def_adhoc_add(B, gcfg.defAdhocChannel));
		ns(Globals_fips_add(B, (gcfg.suppInfo & SUPPINFO_FIPS)));
		ns(Globals_pmk_add(B, gcfg.PMKcaching));
		ns(Globals_probe_delay_add(B, gcfg.probeDelay));
		ns(Globals_regdomain_add(B, gcfg.regDomain));
		ns(Globals_roam_period_add(B, gcfg.roamPeriod));
		ns(Globals_roam_trigger_add(B, gcfg.roamTrigger));
		ns(Globals_rts_add(B, gcfg.RTSThreshold));
		ns(Globals_scan_dfs_add(B, gcfg.scanDFSTime));
		ns(Globals_ttls_add(B, gcfg.TTLSInnerMethod));
		ns(Globals_uapsd_add(B, gcfg.uAPSD));
		ns(Globals_wmm_add(B, gcfg.WMEenabled));
		ns(Globals_ignore_null_ssid_add(B, ignoreNullSSID));
		ns(Globals_dfs_channels_add(B, gcfg.DFSchannels));

		ns(Globals_end_as_root(B));
	}
	return 0;
}

//return codes:
//0 - success
//positive value - benign error
//negative value - unrecoverable error
int do_set_globals(flatcc_builder_t *B, ns(Command_table_t) cmd, pthread_mutex_t *sdk_lock)
{
	ns(Globals_table_t) gt;
	SDCGlobalConfig gcfg = {0};
	int ret;

	//TODO we ought to do some assertion that the cmd_table is a globals
	gt = ns(Command_cmd_pl(cmd));

	SDKLOCK(sdk_lock);
	gcfg.authServerType = ns(Globals_auth(gt));
	gcfg.aLRS = ns(Globals_channel_set_a(gt));
	gcfg.bLRS = ns(Globals_channel_set_b(gt));
	ret = LRD_WF_AutoProfileControl((unsigned char)ns(Globals_auto_profile(gt)));
	gcfg.BeaconMissTimeout = ns(Globals_beacon_miss(gt));
	gcfg.BTcoexist = ns(Globals_bt_coex(gt));
	gcfg.CCXfeatures = ns(Globals_ccx(gt));
	strncpy(gcfg.certPath, ns(Globals_cert_path(gt)), MAX_CERT_PATH);
	if(ns(Globals_date_check(gt)))
		gcfg.suppInfo |= SUPPINFO_TLS_TIME_CHECK;
	gcfg.defAdhocChannel = ns(Globals_def_adhoc(gt));
	if (ns(Globals_fips(gt)))
		gcfg.suppInfo |= SUPPINFO_FIPS;
	gcfg.PMKcaching = ns(Globals_pmk(gt));
	gcfg.probeDelay = ns(Globals_probe_delay(gt));
	gcfg.regDomain = ns(Globals_regdomain(gt));
	gcfg.roamPeriod = ns(Globals_roam_period(gt));
	gcfg.roamTrigger = ns(Globals_roam_trigger(gt));
	gcfg.RTSThreshold = ns(Globals_rts(gt));
	gcfg.scanDFSTime = ns(Globals_scan_dfs(gt));
	gcfg.TTLSInnerMethod = ns(Globals_ttls(gt));
	gcfg.uAPSD = ns(Globals_uapsd(gt));
	gcfg.WMEenabled = ns(Globals_wmm(gt));
	if(!ret)
		ret = setIgnoreNullSsid((unsigned long) ns(Globals_ignore_null_ssid(gt)));
	gcfg.DFSchannels = ns(Globals_dfs_channels(gt));

	SDKUNLOCK(sdk_lock);

	build_handshake_ack(B, ret);
	return 0;
}

//return codes:
//0 - success
//positive value - benign error
//negative value - unrecoverable error
int do_issue_ntpdate(flatcc_builder_t *B, ns(Command_table_t) ct)
{
	ns(String_table_t) string;
	int ret = DCAL_SUCCESS;
	string = ns(Command_cmd_pl(ct));
	char *cmd = NULL;
	FILE *fp = NULL;

	if (((char*)ns(String_value(string)))==NULL)
		ret = DCAL_INVALID_PARAMETER;
	else
	{
#define NTPDATE "/usr/bin/ntpdate "
		cmd = (char*)malloc(strlen(NTPDATE)+
		                    strlen((char*)ns(String_value(string)))+2);
		if (cmd==NULL)
			ret = DCAL_HOST_INSUFFICIENT_MEMORY;
		else
		{
			sprintf(cmd, "%s%s", NTPDATE, (char*)ns(String_value(string)));
			DBGDEBUG("Issuing: %s\n", cmd);

			fp = popen(cmd, "w");
			if (fp==NULL) {
				DBGDEBUG("popen error\n");
				ret = DCAL_HOST_GENERAL_FAIL;
			} else {
				ret = pclose(fp);
				if (ret == -1) {
					DBGDEBUG("pclose error\n");
					ret = DCAL_HOST_GENERAL_FAIL;
				}
				else
					ret = WEXITSTATUS(ret);
			}
		}
	}

	build_handshake_ack(B, ret);
	if (cmd)
		free(cmd);
	return 0;
}

//return codes:
//0 - success
//positive value - benign error
//negative value - unrecoverable error
int do_get_time(flatcc_builder_t *B)
{
	struct timeval tv;

	if (!gettimeofday(&tv, NULL))
	{
		flatcc_builder_reset(B);
		flatbuffers_buffer_start(B, ns(Time_type_identifier));
		ns(Time_start(B));
		ns(Time_tv_sec_add(B, tv.tv_sec));
		ns(Time_tv_usec_add(B, tv.tv_usec));

		ns(Time_end_as_root(B));
	} else
		build_handshake_ack(B, DCAL_HOST_GENERAL_FAIL);

	return 0;
}

//return codes:
//0 - success
//positive value - benign error
//negative value - unrecoverable error
int do_set_time(flatcc_builder_t *B, ns(Command_table_t) cmd)
{
	struct timeval tv;
	int ret = DCAL_SUCCESS;
	ns(Time_table_t) tt;

	tt = ns(Command_cmd_pl(cmd));

	tv.tv_sec = ns(Time_tv_sec(tt));
	tv.tv_usec = ns(Time_tv_usec(tt));

	if (settimeofday(&tv, NULL))
		ret = DCAL_HOST_GENERAL_FAIL;

	build_handshake_ack(B, ret);
	return 0;
}

//return codes:
//0 - success
//positive value - benign error
//negative value - unrecoverable error
int process_command(flatcc_builder_t *B, ns(Command_table_t) cmd,
 pthread_mutex_t *sdk_lock, bool *exit_called)
{
	switch(ns(Command_command(cmd))){
		case ns(Commands_GETSTATUS):
			DBGDEBUG("Get Status\n");
			return build_status(B, sdk_lock);
			break;
		case ns(Commands_GETVERSIONS):
			DBGDEBUG("Get Version\n");
			return build_version(B, sdk_lock);
			break;
		case ns(Commands_WIFIENABLE):
		case ns(Commands_WIFIDISABLE):
			DBGDEBUG("%s\n",
			                ns(Command_command(cmd))==ns(Commands_WIFIENABLE)?
			                "enable":"disable");
			return do_enable_disable(B, sdk_lock,
			          ns(Command_command(cmd))==ns(Commands_WIFIENABLE));
			break;
		case ns(Commands_SETPROFILE):
			DBGDEBUG("set profile\n");
			return do_set_profile(B, cmd, sdk_lock);
			break;
		case ns(Commands_ACTIVATEPROFILE):
			DBGDEBUG("activate profile\n");
			return do_activate_profile(B, cmd, sdk_lock);
			break;
		case ns(Commands_WIFIRESTART):
			DBGDEBUG("wifi restart\n");
			return do_issue_radiorestart(B, sdk_lock);
			break;
		case ns(Commands_SYSTEMREBOOT):
			DBGDEBUG("system reboot\n");
			build_handshake_ack(B, 0);
			*exit_called = true;
			return 0;
			break;
		case ns(Commands_GETPROFILE):
			DBGDEBUG("Get profile\n");
			return do_get_profile(B, cmd, sdk_lock);
			break;
		case ns(Commands_DELPROFILE):
			DBGDEBUG("Del profile\n");
			return do_del_profile(B, cmd, sdk_lock);
			break;
			case ns(Commands_GETGLOBALS):
			DBGDEBUG("Get Globals\n");
			return do_get_globals(B, sdk_lock);
			break;
			case ns(Commands_SETGLOBALS):
			DBGDEBUG("Set Globals\n");
			return do_set_globals(B, cmd, sdk_lock);
			break;
			case ns(Commands_SETTIME):
			DBGDEBUG("Set Time\n");
			return do_set_time(B, cmd);
			break;
			case ns(Commands_GETTIME):
			DBGDEBUG("Get Time\n");
			return do_get_time(B);
			break;
			case ns(Commands_NTPDATE):
			DBGDEBUG("NTPDATE\n");
			return do_issue_ntpdate(B, cmd);
			break;
//TODO - add other command processing
		case ns(Commands_GETPROFILELIST):
			return do_get_profile_list(B, sdk_lock);
			break;
		default:
			return SDCERR_NOT_IMPLEMENTED;
	}
}

// the passed in buffer is used for the outbound buffer as well.  The
// buffer size is buf_size while the number of bytes used for inbound
// is nbytes, the number of bytes used in the outbound buffer is the
// return code. However, if the return is negative, this is an error
// that is unrecoverable and the session should be ended. (The buffer's
// content on a unrecoverable error is undefined.) A
// recoverable error is handled by putting a NACK in the return buffer
// and the error in the returned handshake table

int process_buffer(char * buf, size_t buf_size, size_t nbytes, pthread_mutex_t *sdk_lock, bool must_be_handshake, bool *exit_called)
{
	flatcc_builder_t builder;
	flatcc_builder_init(&builder);
	int ret;
	flatbuffers_thash_t buftype;

	//hexdump("read buffer", buf, nbytes, stdout);

	buftype = verify_buffer(buf, nbytes);
	if (buftype==0){
		DBGERROR("could not verify buffer.  Sending NACK\n");
		ret = DCAL_FLATBUFF_VALIDATION_FAIL;
		goto respond_with_nack;
	}

	DBGINFO("incoming buffer has type: %s\n", buftype_to_string(buftype));

	if ((must_be_handshake) && (buftype != ns(Handshake_type_hash))){
		DBGERROR("wanted a handshake but this is: %s\n", buftype_to_string(buftype));
		ret = DCAL_FLATBUFF_VALIDATION_FAIL;
		goto respond_with_nack;
	}

	switch(buftype) {
		case ns(Handshake_type_hash):
			DBGINFO("inbound handshake buffer received\n");
			if (is_handshake_valid(ns(Handshake_as_root(buf)))) {
				DBGINFO("Got good protocol HELLO\n");
				build_handshake_ack(&builder, 0);
				goto respond_normal;
			}
			// not a valid handshake - respond with nack
			ret = DCAL_FLATBUFF_VALIDATION_FAIL;
			goto respond_with_nack;
			break;
		case ns(Command_type_hash):
			// process command
			if ((ret=process_command(&builder, ns(Command_as_root(buf)), sdk_lock, exit_called))){
				// un-recoverable errors will be negative
				if (ret > 0)
					goto respond_with_nack;
				// unrecoverable error
				nbytes = ret;
				goto respond_with_error;
			}
			// no error
			goto respond_normal;
			break;
		default:
			DBGINFO("failed to get HELLO\n");
			ret = DCAL_FLATBUFF_ERROR;
			goto respond_with_nack;
	}

respond_with_nack:
	build_handshake_ack(&builder, ret);
respond_normal:
	flatcc_builder_copy_buffer(&builder, buf, buf_size);
	nbytes =flatcc_builder_get_buffer_size(&builder);
	DBGDEBUG("Created response buffer size: %zd\n", nbytes);
	hexdump("outbound buffer", buf, nbytes, stdout);
respond_with_error: // allow for exit with 0 or negative return
	flatcc_builder_clear(&builder);
	return nbytes;
}
