/*
 * partial PKCS15 emulation for PIV-II cards
 * only minimal use of the authentication cert and key
 *
 * Copyright (C) 2005,2006,2007,2008,2009,2010  
 *               Douglas E. Engert <deengert@anl.gov> 
 *               2004, Nils Larsch <larsch@trustcenter.de>
 * Copyright (C) 2006, Identity Alliance, 
 *               Thomas Harning <thomas.harning@identityalliance.com>
 * Copyright (C) 2007, EMC, Russell Larner <rlarner@rsa.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "internal.h"
#include "cardctl.h"
#include "pkcs15.h"

#define MANU_ID		"piv_II "

int sc_pkcs15emu_piv_init_ex(sc_pkcs15_card_t *, sc_pkcs15emu_opt_t *);


typedef struct objdata_st {
	const char *id;
	const char *label;
	const char *aoid;
	const char *auth_id;
	const char *path;
	int         obj_flags;
} objdata;

typedef struct cdata_st {
	const char *id;
	const char *label;
	int	    authority;
	const char *path;
	int         obj_flags;
} cdata;

typedef struct pdata_st {
	const char *id;
	const char *label;
	const char *path;
	int         ref;
	int         type;
	unsigned int maxlen;
	unsigned int minlen;
	unsigned int storedlen;
	int         flags;	
	int         tries_left;
	const unsigned char  pad_char;
	int         obj_flags;
} pindata; 

typedef struct pubdata_st {
	const char *id;
	const char *label;
	int         usage;
	const char *path;
	int         ref;
	const char *auth_id;
	int         obj_flags;
} pubdata;

typedef struct prdata_st {
	const char *id;
	const char *label;
	int         usage;
	const char *path;
	int         ref;
	const char *auth_id;
	int         obj_flags;
} prdata;

typedef struct common_key_info_st {
	int cert_found;
	int pubkey_found;
	int key_alg;
	unsigned int modulus_len;
	int not_present;
} common_key_info;

static int piv_detect_card(sc_pkcs15_card_t *p15card)
{
	sc_card_t *card = p15card->card;

	SC_FUNC_CALLED(card->ctx, SC_LOG_DEBUG_VERBOSE);
	if (card->type < SC_CARD_TYPE_PIV_II_GENERIC
		|| card->type >= SC_CARD_TYPE_PIV_II_GENERIC+1000)
		return SC_ERROR_INVALID_CARD;
	return SC_SUCCESS;
}

static int sc_pkcs15emu_piv_init(sc_pkcs15_card_t *p15card)
{

	/* The cert objects will return all the data */
	/* Note: pkcs11 objects do not have CK_ID values */

	static const objdata objects[] = {
	{"1", "Card Capability Container", 
			"2.16.840.1.101.3.7.1.219.0", NULL, "DB00", 0},
	{"2", "Card Holder Unique Identifier",
			"2.16.840.1.101.3.7.2.48.0", NULL, "3000", 0},
	{"3", "Unsigned Card Holder Unique Identifier",
			"2.16.840.1.101.3.7.2.48.2", NULL, "3010", 0},
	{"4", "X.509 Certificate for PIV Authentication",
			"2.16.840.1.101.3.7.2.1.1", NULL, "0101", 0},
	{"5", "Cardholder Fingerprints",
			"2.16.840.1.101.3.7.2.96.16", "1", "6010", SC_PKCS15_CO_FLAG_PRIVATE},
	{"6", "Printed Information",
			"2.16.840.1.101.3.7.2.48.1", "1", "3001", SC_PKCS15_CO_FLAG_PRIVATE},
	{"7", "Cardholder Facial Image", 
			"2.16.840.1.101.3.7.2.96.48", "1", "6030", SC_PKCS15_CO_FLAG_PRIVATE},
	{"8", "X.509 Certificate for Digital Signature",
			"2.16.840.1.101.3.7.2.1.0",  NULL, "0100", 0},
	{"9", "X.509 Certificate for Key Management", 
			"2.16.840.1.101.3.7.2.1.2", NULL, "0102", 0},
	{"10","X.509 Certificate for Card Authentication",
			"2.16.840.1.101.3.7.2.5.0", NULL, "0500", 0},
	{"11", "Security Object",
			"2.16.840.1.101.3.7.2.144.0", NULL, "9000", 0},
	{"12", "Discovery Object",
			"2.16.840.1.101.3.7.2.96.80", NULL, "6050", 0},
	{"13", "Key History Object",
			"2.16.840.1.101.3.7.2.96.96", NULL, "6060", 0},
	{"14", "Cardholder Iris Image",
			"2.16.840.1.101.3.7.2.16.21", NULL, "1015", SC_PKCS15_CO_FLAG_PRIVATE},

	{"15", "Retired X.509 Certificate for Key Management 1", 
			"2.16.840.1.101.3.7.2.16.1", NULL, "1001", 0},
	{"16", "Retired X.509 Certificate for Key Management 2", 
			"2.16.840.1.101.3.7.2.16.2", NULL, "1002", 0},
	{"17", "Retired X.509 Certificate for Key Management 3", 
			"2.16.840.1.101.3.7.2.16.3", NULL, "1003", 0},
	{"18", "Retired X.509 Certificate for Key Management 4", 
			"2.16.840.1.101.3.7.2.16.4", NULL, "1004", 0},
	{"19", "Retired X.509 Certificate for Key Management 5", 
			"2.16.840.1.101.3.7.2.16.5", NULL, "1005", 0},
	{"20", "Retired X.509 Certificate for Key Management 6", 
			"2.16.840.1.101.3.7.2.16.6", NULL, "1006", 0},
	{"21", "Retired X.509 Certificate for Key Management 7", 
			"2.16.840.1.101.3.7.2.16.7", NULL, "1007", 0},
	{"22", "Retired X.509 Certificate for Key Management 8", 
			"2.16.840.1.101.3.7.2.16.8", NULL, "1008", 0},
	{"23", "Retired X.509 Certificate for Key Management 9", 
			"2.16.840.1.101.3.7.2.16.9", NULL, "1009", 0},
	{"24", "Retired X.509 Certificate for Key Management 10", 
			"2.16.840.1.101.3.7.2.16.10", NULL, "100A", 0},
	{"25", "Retired X.509 Certificate for Key Management 11", 
			"2.16.840.1.101.3.7.2.16.11", NULL, "100B", 0},
	{"26", "Retired X.509 Certificate for Key Management 12", 
			"2.16.840.1.101.3.7.2.16.12", NULL, "100C", 0},
	{"27", "Retired X.509 Certificate for Key Management 13", 
			"2.16.840.1.101.3.7.2.16.13", NULL, "100D", 0},
	{"28", "Retired X.509 Certificate for Key Management 14", 
			"2.16.840.1.101.3.7.2.16.14", NULL, "100E", 0},
	{"29", "Retired X.509 Certificate for Key Management 15", 
			"2.16.840.1.101.3.7.2.16.15", NULL, "100F", 0},
	{"30", "Retired X.509 Certificate for Key Management 16", 
			"2.16.840.1.101.3.7.2.16.16", NULL, "1010", 0},
	{"31", "Retired X.509 Certificate for Key Management 17", 
			"2.16.840.1.101.3.7.2.16.17", NULL, "1011", 0},
	{"32", "Retired X.509 Certificate for Key Management 18", 
			"2.16.840.1.101.3.7.2.16.18", NULL, "1012", 0},
	{"33", "Retired X.509 Certificate for Key Management 19", 
			"2.16.840.1.101.3.7.2.16.19", NULL, "1013", 0},
	{"34", "Retired X.509 Certificate for Key Management 20", 
			"2.16.840.1.101.3.7.2.16.20", NULL, "1014", 0},
	{NULL, NULL, NULL, NULL, NULL, 0}
};
	/* 
	 * NIST 800-73-1 lifted the restriction on 
	 * requering pin protected certs. Thus the default is to   
	 * not require this.
	 */
	/* certs will be pulled out from the cert objects */
	/* the number of cert, pubkey and prkey triplets */

#define PIV_NUM_CERTS_AND_KEYS 24

	static const cdata certs[PIV_NUM_CERTS_AND_KEYS] = {
		{"1", "Certificate for PIV Authentication", 0, "0101cece", 0},
		{"2", "Certificate for Digital Signature", 0, "0100cece", 0},
		{"3", "Certificate for Key Management", 0, "0102cece", 0},
		{"4", "Certificate for Card Authentication", 0, "0500cece", 0},
		{"5", "Retired Certificate for Key Management 1", 0, "1001cece", 0},
		{"6", "Retired Certificate for Key Management 2", 0, "1002cece", 0},
		{"7", "Retired Certificate for Key Management 3", 0, "1003cece", 0},
		{"8", "Retired Certificate for Key Management 4", 0, "1004cece", 0},
		{"9", "Retired Certificate for Key Management 5", 0, "1005cece", 0},
		{"10", "Retired Certificate for Key Management 6", 0, "1006cece", 0},
		{"11", "Retired Certificate for Key Management 7", 0, "1007cece", 0},
		{"12", "Retired Certificate for Key Management 8", 0, "1008cece", 0},
		{"13", "Retired Certificate for Key Management 9", 0, "1009cece", 0},
		{"14", "Retired Certificate for Key Management 10", 0, "100Acece", 0},
		{"15", "Retired Certificate for Key Management 11", 0, "100Bcece", 0},
		{"16", "Retired Certificate for Key Management 12", 0, "100Ccece", 0},
		{"17", "Retired Certificate for Key Management 13", 0, "100Dcece", 0},
		{"18", "Retired Certificate for Key Management 14", 0, "100Ecece", 0},
		{"19", "Retired Certificate for Key Management 15", 0, "100Fcece", 0},
		{"20", "Retired Certificate for Key Management 16", 0, "1010cece", 0},
		{"21", "Retired Certificate for Key Management 17", 0, "1011cece", 0},
		{"22", "Retired Certificate for Key Management 18", 0, "1012cece", 0},
		{"23", "Retired Certificate for Key Management 19", 0, "1013cece", 0},
		{"24", "Retired Certificate for Key Management 20", 0, "1014cece", 0}
	};

	static const pindata pins[] = {
		{ "1", "PIV Card Holder pin", "", 0x80,
		  /* label and ref will change if using global pin */
		  SC_PKCS15_PIN_TYPE_ASCII_NUMERIC,
		  8, 4, 8, 
		  SC_PKCS15_PIN_FLAG_NEEDS_PADDING |
		  SC_PKCS15_PIN_FLAG_LOCAL, 
		  -1, 0xFF,
		  SC_PKCS15_CO_FLAG_PRIVATE },
		{ "2", "PIV PUK", "", 0x81, 
		  SC_PKCS15_PIN_TYPE_ASCII_NUMERIC,
		  8, 4, 8, 
		  SC_PKCS15_PIN_FLAG_NEEDS_PADDING |
		  SC_PKCS15_PIN_FLAG_LOCAL | SC_PKCS15_PIN_FLAG_SO_PIN |
		  SC_PKCS15_PIN_FLAG_UNBLOCKING_PIN, 
		  -1, 0xFF, 
		  SC_PKCS15_CO_FLAG_PRIVATE },
		{ NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0}
	};


	/*
	 * The size of the key or the algid is not really known
	 * but can be derived from the certificates. 
	 * the cert, pubkey and privkey are a set. 
	 * Key usages bits taken from pkcs15v1_1 Table 2
	 */
	static const pubdata pubkeys[PIV_NUM_CERTS_AND_KEYS] = {

		{ "1", "PIV AUTH pubkey", 
			 	SC_PKCS15_PRKEY_USAGE_ENCRYPT |
			 	SC_PKCS15_PRKEY_USAGE_WRAP |
				SC_PKCS15_PRKEY_USAGE_VERIFY |
				SC_PKCS15_PRKEY_USAGE_VERIFYRECOVER,
			"9A06", 0x9A, "1", 0},
		{ "2", "SIGN pubkey", 
				SC_PKCS15_PRKEY_USAGE_ENCRYPT |
				SC_PKCS15_PRKEY_USAGE_VERIFY |
				SC_PKCS15_PRKEY_USAGE_VERIFYRECOVER |
				SC_PKCS15_PRKEY_USAGE_NONREPUDIATION,
			"9C06", 0x9C, "1", 0},
		{ "3", "KEY MAN pubkey", 
				SC_PKCS15_PRKEY_USAGE_WRAP,
			"9D06", 0x9D, "1", 0},
		{ "4", "CARD AUTH pubkey", 
				SC_PKCS15_PRKEY_USAGE_VERIFY |
				SC_PKCS15_PRKEY_USAGE_VERIFYRECOVER, 
			"9E06", 0x9E, "0", 0},  /* no pin, and avail in contactless */

		{ "5", "Retired KEY MAN 1",
				SC_PKCS15_PRKEY_USAGE_WRAP,
			 "8206", 0x82, "1", 0},
		{ "6", "Retired KEY MAN 2",
				SC_PKCS15_PRKEY_USAGE_WRAP,
			 "8306", 0x83, "1", 0},
		{ "7", "Retired KEY MAN 3",
				SC_PKCS15_PRKEY_USAGE_WRAP,
			 "8406", 0x84, "1", 0},
		{ "8", "Retired KEY MAN 4",
				SC_PKCS15_PRKEY_USAGE_WRAP,
			 "8506", 0x85, "1", 0},
		{ "9", "Retired KEY MAN 5",
				SC_PKCS15_PRKEY_USAGE_WRAP,
			 "8606", 0x86, "1", 0},
		{ "10", "Retired KEY MAN 6",
				SC_PKCS15_PRKEY_USAGE_WRAP,
			 "8706", 0x87, "1", 0},
		{ "11", "Retired KEY MAN 7",
				SC_PKCS15_PRKEY_USAGE_WRAP,
			 "8806", 0x88, "1", 0},
		{ "12", "Retired KEY MAN 8",
				SC_PKCS15_PRKEY_USAGE_WRAP,
			 "8906", 0x89, "1", 0},
		{ "13", "Retired KEY MAN 9",
				SC_PKCS15_PRKEY_USAGE_WRAP,
			 "8A06", 0x8A, "1", 0},
		{ "14", "Retired KEY MAN 10",
				SC_PKCS15_PRKEY_USAGE_WRAP,
			 "8B06", 0x8B, "1", 0},
		{ "15", "Retired KEY MAN 11",
				SC_PKCS15_PRKEY_USAGE_WRAP,
			 "8C06", 0x8C, "1", 0},
		{ "16", "Retired KEY MAN 12",
				SC_PKCS15_PRKEY_USAGE_WRAP,
			 "8D06", 0x8D, "1", 0},
		{ "17", "Retired KEY MAN 13",
				SC_PKCS15_PRKEY_USAGE_WRAP,
			 "8E06", 0x8E, "1", 0},
		{ "18", "Retired KEY MAN 14",
				SC_PKCS15_PRKEY_USAGE_WRAP,
			 "8F06", 0x8F, "1", 0},
		{ "19", "Retired KEY MAN 15",
				SC_PKCS15_PRKEY_USAGE_WRAP,
			 "9006", 0x90, "1", 0},
		{ "20", "Retired KEY MAN 16",
				SC_PKCS15_PRKEY_USAGE_WRAP,
			 "9106", 0x91, "1", 0},
		{ "21", "Retired KEY MAN 17",
				SC_PKCS15_PRKEY_USAGE_WRAP,
			 "9206", 0x92, "1", 0},
		{ "22", "Retired KEY MAN 18",
				SC_PKCS15_PRKEY_USAGE_WRAP,
			 "9306", 0x93, "1", 0},
		{ "23", "Retired KEY MAN 19",
				SC_PKCS15_PRKEY_USAGE_WRAP,
			 "9406", 0x94, "1", 0},
		{ "24", "Retired KEY MAN 20",
				SC_PKCS15_PRKEY_USAGE_WRAP,
			 "9506", 0x95, "1", 0}
	};

	static const prdata prkeys[PIV_NUM_CERTS_AND_KEYS] = {
		{ "1", "PIV AUTH key", 
				SC_PKCS15_PRKEY_USAGE_DECRYPT |
				SC_PKCS15_PRKEY_USAGE_UNWRAP |
				SC_PKCS15_PRKEY_USAGE_SIGN |
				SC_PKCS15_PRKEY_USAGE_SIGNRECOVER,
			"", 0x9A, "1", 0},
		{ "2", "SIGN key", 
				SC_PKCS15_PRKEY_USAGE_DECRYPT |
				SC_PKCS15_PRKEY_USAGE_SIGN |
				SC_PKCS15_PRKEY_USAGE_SIGNRECOVER |
				SC_PKCS15_PRKEY_USAGE_NONREPUDIATION,
			"", 0x9C, "1", 0},
		{ "3", "KEY MAN key", 
				SC_PKCS15_PRKEY_USAGE_UNWRAP,
			"", 0x9D, "1", 0},
		{ "4", "CARD AUTH key", 
				SC_PKCS15_PRKEY_USAGE_SIGN |
				SC_PKCS15_PRKEY_USAGE_SIGNRECOVER,
			"", 0x9E, NULL, 0}, /* no PIN needed, works with wireless */
		{ "5", "Retired KEY MAN 1",
				SC_PKCS15_PRKEY_USAGE_UNWRAP,
			"", 0x82, "1", 0},
		{ "6", "Retired KEY MAN 2",
				SC_PKCS15_PRKEY_USAGE_UNWRAP,
			"", 0x83, "1", 0},
		{ "7", "Retired KEY MAN 3",
				SC_PKCS15_PRKEY_USAGE_UNWRAP,
			"", 0x84, "1", 0},
		{ "8", "Retired KEY MAN 4",
				SC_PKCS15_PRKEY_USAGE_UNWRAP,
			"", 0x85, "1", 0},
		{ "9", "Retired KEY MAN 5",
				SC_PKCS15_PRKEY_USAGE_UNWRAP,
			"", 0x86, "1", 0},
		{ "10", "Retired KEY MAN 6",
				SC_PKCS15_PRKEY_USAGE_UNWRAP,
			"", 0x87, "1", 0},
		{ "11", "Retired KEY MAN 7",
				SC_PKCS15_PRKEY_USAGE_UNWRAP,
			"", 0x88, "1", 0},
		{ "12", "Retired KEY MAN 8",
				SC_PKCS15_PRKEY_USAGE_UNWRAP,
			"", 0x89, "1", 0},
		{ "13", "Retired KEY MAN 9",
				SC_PKCS15_PRKEY_USAGE_UNWRAP,
			"", 0x8A, "1", 0},
		{ "14", "Retired KEY MAN 10",
				SC_PKCS15_PRKEY_USAGE_UNWRAP,
			"", 0x8B, "1", 0},
		{ "15", "Retired KEY MAN 11",
				SC_PKCS15_PRKEY_USAGE_UNWRAP,
			"", 0x8C, "1", 0},
		{ "16", "Retired KEY MAN 12",
				SC_PKCS15_PRKEY_USAGE_UNWRAP,
			"", 0x8D, "1", 0},
		{ "17", "Retired KEY MAN 13",
				SC_PKCS15_PRKEY_USAGE_UNWRAP,
			"", 0x8E, "1", 0},
		{ "18", "Retired KEY MAN 14",
				SC_PKCS15_PRKEY_USAGE_UNWRAP,
			"", 0x8F, "1", 0},
		{ "19", "Retired KEY MAN 15",
				SC_PKCS15_PRKEY_USAGE_UNWRAP,
			"", 0x90, "1", 0},
		{ "20", "Retired KEY MAN 16",
				SC_PKCS15_PRKEY_USAGE_UNWRAP,
			"", 0x91, "1", 0},
		{ "21", "Retired KEY MAN 17",
				SC_PKCS15_PRKEY_USAGE_UNWRAP,
			"", 0x92, "1", 0},
		{ "22", "Retired KEY MAN 18",
				SC_PKCS15_PRKEY_USAGE_UNWRAP,
			"", 0x93, "1", 0},
		{ "23", "Retired KEY MAN 19",
				SC_PKCS15_PRKEY_USAGE_UNWRAP,
			"", 0x94, "1", 0},
		{ "24", "Retired KEY MAN 20",
				SC_PKCS15_PRKEY_USAGE_UNWRAP,
			"", 0x95, "1", 0}
	};

	int    r, i;
	sc_card_t *card = p15card->card;
	sc_file_t *file_out = NULL;
	int exposed_cert[PIV_NUM_CERTS_AND_KEYS] = {1, 0, 0, 0};
	sc_serial_number_t serial;
	char buf[SC_MAX_SERIALNR * 2 + 1];
	common_key_info ckis[PIV_NUM_CERTS_AND_KEYS];


	SC_FUNC_CALLED(card->ctx, SC_LOG_DEBUG_VERBOSE);

	/* could read this off card if needed */

	/* CSP does not like a - in the name */
	p15card->tokeninfo->label = strdup("PIV_II");
	p15card->tokeninfo->manufacturer_id = strdup(MANU_ID);

	/*
	 * get serial number 
	 * We will use the FASC-N from the CHUID
	 * Note we are not verifying CHUID, belongs to this card
	 * but need serial number for Mac tokend 
	 */

	r = sc_card_ctl(card, SC_CARDCTL_GET_SERIALNR, &serial);
	if (r < 0) {
		sc_debug(card->ctx, SC_LOG_DEBUG_NORMAL,"sc_card_ctl rc=%d",r);
		p15card->tokeninfo->serial_number = strdup("00000000");
	} else {
		sc_bin_to_hex(serial.value, serial.len, buf, sizeof(buf), 0);
		p15card->tokeninfo->serial_number = strdup(buf);
	}

	sc_debug(card->ctx, SC_LOG_DEBUG_NORMAL, "PIV-II adding objects...");

	/* set other objects */
	for (i = 0; objects[i].label; i++) {
		struct sc_pkcs15_data_info obj_info;
		struct sc_pkcs15_object    obj_obj;

		memset(&obj_info, 0, sizeof(obj_info));
		memset(&obj_obj, 0, sizeof(obj_obj));
		sc_pkcs15_format_id(objects[i].id, &obj_info.id);
		sc_format_path(objects[i].path, &obj_info.path);

		/* See if the object can not be present on the card */
		r = (card->ops->card_ctl)(card, SC_CARDCTL_PIV_OBJECT_PRESENT, &obj_info.path);
		if (r == 1)
			continue; /* Not on card, do not define the object */
			
		strncpy(obj_info.app_label, objects[i].label, SC_PKCS15_MAX_LABEL_SIZE - 1);
		r = sc_format_oid(&obj_info.app_oid, objects[i].aoid);
		if (r != SC_SUCCESS)
			return r;

		if (objects[i].auth_id)
			sc_pkcs15_format_id(objects[i].auth_id, &obj_obj.auth_id);

		strncpy(obj_obj.label, objects[i].label, SC_PKCS15_MAX_LABEL_SIZE - 1);
		obj_obj.flags = objects[i].obj_flags;
		
		r = sc_pkcs15emu_object_add(p15card, SC_PKCS15_TYPE_DATA_OBJECT, 
			&obj_obj, &obj_info); 
		if (r < 0)
			SC_FUNC_RETURN(card->ctx, SC_LOG_DEBUG_NORMAL, r);
	}

	/*
	 * certs, pubkeys and priv keys are related and we assume
	 * they are in order 
	 * We need to read the cert, get modulus and keylen 
	 * We use those for the pubkey, and priv key objects. 
	 * If no cert, then see if pubkey (i.e. we are initilizing,
	 * and the pubkey is in a file,) then add pubkey and privkey
	 * If no cert and no pubkey, skip adding them. 
 
	 */
	/* set certs */
	sc_debug(card->ctx, SC_LOG_DEBUG_NORMAL, "PIV-II adding certs...");
	for (i = 0; i < PIV_NUM_CERTS_AND_KEYS; i++) {
		struct sc_pkcs15_cert_info cert_info;
		struct sc_pkcs15_object    cert_obj;
		sc_pkcs15_der_t   cert_der;
		sc_pkcs15_cert_t *cert_out;
		
		ckis[i].cert_found = 0;
		ckis[i].key_alg = -1;
		ckis[i].pubkey_found = 0;
		ckis[i].modulus_len = 0;

		if ((card->flags & 0x20) &&  (exposed_cert[i] == 0))
			continue;

		memset(&cert_info, 0, sizeof(cert_info));
		memset(&cert_obj,  0, sizeof(cert_obj));
	
		sc_pkcs15_format_id(certs[i].id, &cert_info.id);
		cert_info.authority = certs[i].authority;
		sc_format_path(certs[i].path, &cert_info.path);

		strncpy(cert_obj.label, certs[i].label, SC_PKCS15_MAX_LABEL_SIZE - 1);
		cert_obj.flags = certs[i].obj_flags;

		/* See if the cert might be present or not. */
		r = (card->ops->card_ctl)(card, SC_CARDCTL_PIV_OBJECT_PRESENT, &cert_info.path);
		if (r == 1) {
			sc_debug(card->ctx, SC_LOG_DEBUG_NORMAL, "Cert can not be present,i=%d", i);
			continue;
		}

		/* use a &file_out so card-piv.c will read cert if present */
		r = sc_pkcs15_read_file(p15card, &cert_info.path, 
				&cert_der.value, &cert_der.len, &file_out);
		if (file_out) {
			sc_file_free(file_out);
			file_out = NULL;
		}

		if (r) { 
			sc_debug(card->ctx, SC_LOG_DEBUG_NORMAL, "No cert found,i=%d", i);
			continue;
		}

		ckis[i].cert_found = 1;
		/* cache it using the PKCS15 emulation objects */
		/* as it does not change */
               	if (cert_der.value) {
               	 	cert_info.value.value = cert_der.value;
                       	cert_info.value.len = cert_der.len;
                       	cert_info.path.len = 0; /* use in mem cert from now on */
               	}
		/* following will find the cached cert in cert_info */
		r =  sc_pkcs15_read_certificate(p15card, &cert_info, &cert_out);
		if (r < 0) {
			sc_debug(card->ctx, SC_LOG_DEBUG_NORMAL, "Failed to read/parse the certificate r=%d",r);
			continue;
		}
		/* TODO support EC keys */
		ckis[i].key_alg = cert_out->key.algorithm;
		if (cert_out->key.algorithm == SC_ALGORITHM_RSA) {
			/* save modulus_len for pub and priv */
			ckis[i].modulus_len = cert_out->key.u.rsa.modulus.len * 8;
		} else {
/*TODO add the SC_ALGORITHM_EC */

			sc_debug(card->ctx, SC_LOG_DEBUG_NORMAL, "Unsuported key.algorithm %d", cert_out->key.algorithm);
			ckis[i].modulus_len = 1024; /* set some value for now */
		}
		sc_pkcs15_free_certificate(cert_out);

		r = sc_pkcs15emu_add_x509_cert(p15card, &cert_obj, &cert_info);
		if (r < 0) {
			sc_debug(card->ctx, SC_LOG_DEBUG_NORMAL, " Failed to add cert obj r=%d",r);
			continue;
		}
	}

	/* set pins */
	sc_debug(card->ctx, SC_LOG_DEBUG_NORMAL, "PIV-II adding pins...");
	for (i = 0; pins[i].label; i++) {
		struct sc_pkcs15_pin_info pin_info;
		struct sc_pkcs15_object   pin_obj;
		const char * label;
		int pin_ref;

		memset(&pin_info, 0, sizeof(pin_info));
		memset(&pin_obj,  0, sizeof(pin_obj));

		sc_pkcs15_format_id(pins[i].id, &pin_info.auth_id);
		pin_info.reference     = pins[i].ref;
		pin_info.flags         = pins[i].flags;
		pin_info.type          = pins[i].type;
		pin_info.min_length    = pins[i].minlen;
		pin_info.stored_length = pins[i].storedlen;
		pin_info.max_length    = pins[i].maxlen;
		pin_info.pad_char      = pins[i].pad_char;
		sc_format_path(pins[i].path, &pin_info.path);
		pin_info.tries_left    = -1;

		label = pins[i].label;
		if (i == 0 &&
			(card->ops->card_ctl)(card, SC_CARDCTL_PIV_PIN_PREFERENCE,
					&pin_ref) == 0 &&
				pin_ref == 0x00) { /* must be 80 for PIV pin, or 00 for Global PIN */
			pin_info.reference = pin_ref;
			label = "Global PIN";
		} 
sc_debug(card->ctx, SC_LOG_DEBUG_NORMAL, "DEE Adding pin %d label=%s",i, label);
		strncpy(pin_obj.label, label, SC_PKCS15_MAX_LABEL_SIZE - 1);
		pin_obj.flags = pins[i].obj_flags;

		r = sc_pkcs15emu_add_pin_obj(p15card, &pin_obj, &pin_info);
		if (r < 0)
			SC_FUNC_RETURN(card->ctx, SC_LOG_DEBUG_NORMAL, r);
	}



	/* set public keys */
	/* We may only need this during initialzation when genkey
	 * gets the pubkey, but it can not be read from the card 
	 * at a later time. The piv-tool can stach in file 
	 */ 
	sc_debug(card->ctx, SC_LOG_DEBUG_NORMAL, "PIV-II adding pub keys...");
	for (i = 0; i < PIV_NUM_CERTS_AND_KEYS; i++) {
		struct sc_pkcs15_pubkey_info pubkey_info;
		struct sc_pkcs15_object     pubkey_obj;
		struct sc_pkcs15_pubkey *p15_key;

		if ((card->flags & 0x20) &&  (exposed_cert[i] == 0))
			continue;

		memset(&pubkey_info, 0, sizeof(pubkey_info));
		memset(&pubkey_obj,  0, sizeof(pubkey_obj));


		sc_pkcs15_format_id(pubkeys[i].id, &pubkey_info.id);
		pubkey_info.usage         = pubkeys[i].usage;
		pubkey_info.native        = 1;
		pubkey_info.key_reference = pubkeys[i].ref;

		sc_format_path(pubkeys[i].path, &pubkey_info.path);

		strncpy(pubkey_obj.label, pubkeys[i].label, SC_PKCS15_MAX_LABEL_SIZE - 1);

		pubkey_obj.flags = pubkeys[i].obj_flags;
		

		if (pubkeys[i].auth_id)
			sc_pkcs15_format_id(pubkeys[i].auth_id, &pubkey_obj.auth_id);

		if (ckis[i].cert_found == 0) { /*  no cert found */
			sc_debug(card->ctx, SC_LOG_DEBUG_NORMAL,"No cert for this pub key i=%d",i);
			/* TODO EC */
			pubkey_obj.type = SC_PKCS15_TYPE_PUBKEY_RSA;
			pubkey_obj.data = &pubkey_info;
			r = sc_pkcs15_read_pubkey(p15card, &pubkey_obj, &p15_key);
				pubkey_obj.data = NULL;
				sc_debug(card->ctx, SC_LOG_DEBUG_NORMAL," READING PUB KEY r=%d",r);
			if (r < 0 ) {
				continue;
			}
			/* Only get here if no cert, and the card-piv.c found 
			 * there is a pub key file. This only happens when trying
			 * initializing a card and have set env to point at file  
			 */
			if (p15_key->algorithm == SC_ALGORITHM_RSA) {
			/* save modulus_len in pub and priv */
			ckis[i].key_alg = SC_ALGORITHM_RSA; 
			ckis[i].modulus_len = p15_key->u.rsa.modulus.len * 8;
			ckis[i].pubkey_found = 1;
			}

		}
		/* TODO need to support EC */
		if (ckis[i].key_alg != SC_ALGORITHM_RSA) {
			sc_debug(card->ctx, SC_LOG_DEBUG_NORMAL,"key_alg for %d not RSA %d",i, ckis[i].key_alg);
			continue;
		}

		pubkey_info.modulus_length = ckis[i].modulus_len;
		strncpy(pubkey_obj.label, pubkeys[i].label, SC_PKCS15_MAX_LABEL_SIZE - 1);

		/* TODO EC keys */
		sc_debug(card->ctx, SC_LOG_DEBUG_NORMAL,"adding pubkey for %d keyalg=%d",i, ckis[i].key_alg);
		r = sc_pkcs15emu_add_rsa_pubkey(p15card, &pubkey_obj, &pubkey_info);
		if (r < 0)
			SC_FUNC_RETURN(card->ctx, SC_LOG_DEBUG_NORMAL, r); /* should not fail */

		ckis[i].pubkey_found = 1;
	}


	/* set private keys */
	sc_debug(card->ctx, SC_LOG_DEBUG_NORMAL, "PIV-II adding private keys...");
	for (i = 0; i < PIV_NUM_CERTS_AND_KEYS; i++) {
		struct sc_pkcs15_prkey_info prkey_info;
		struct sc_pkcs15_object     prkey_obj;

		if ((card->flags & 0x20) &&  (exposed_cert[i] == 0))
			continue;

		memset(&prkey_info, 0, sizeof(prkey_info));
		memset(&prkey_obj,  0, sizeof(prkey_obj));

		if (ckis[i].cert_found == 0 && ckis[i].pubkey_found == 0)
			continue; /* i.e. no cert or pubkey */
		
		sc_pkcs15_format_id(prkeys[i].id, &prkey_info.id);
		prkey_info.usage         = prkeys[i].usage;
		prkey_info.native        = 1;
		prkey_info.key_reference = prkeys[i].ref;
		prkey_info.modulus_length= ckis[i].modulus_len;
		/* The cert or pubkey should have filled modulus_len */
		/* TODO EC keys */
		sc_format_path(prkeys[i].path, &prkey_info.path);

		strncpy(prkey_obj.label, prkeys[i].label, SC_PKCS15_MAX_LABEL_SIZE - 1);

		prkey_obj.flags = prkeys[i].obj_flags;

		if (prkeys[i].auth_id)
			sc_pkcs15_format_id(prkeys[i].auth_id, &prkey_obj.auth_id);

		r = sc_pkcs15emu_add_rsa_prkey(p15card, &prkey_obj, &prkey_info);
		if (r < 0)
			SC_FUNC_RETURN(card->ctx, SC_LOG_DEBUG_NORMAL, r);
	}

	SC_FUNC_RETURN(card->ctx, SC_LOG_DEBUG_NORMAL, SC_SUCCESS);
}

int sc_pkcs15emu_piv_init_ex(sc_pkcs15_card_t *p15card,
				  sc_pkcs15emu_opt_t *opts)
{
	sc_card_t   *card = p15card->card;
	sc_context_t    *ctx = card->ctx;

	SC_FUNC_CALLED(ctx, SC_LOG_DEBUG_VERBOSE);

	if (opts && opts->flags & SC_PKCS15EMU_FLAGS_NO_CHECK)
		return sc_pkcs15emu_piv_init(p15card);
	else {
		int r = piv_detect_card(p15card);
		if (r)
			return SC_ERROR_WRONG_CARD;
		return sc_pkcs15emu_piv_init(p15card);
	}
}
