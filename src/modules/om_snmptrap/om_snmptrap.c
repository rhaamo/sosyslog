/*	$Id: om_snmptrap.c,v 1.2 2002/09/17 05:20:28 alejo Exp $	*/

/*
 * Copyright (c) 2002, Core SDI S.A., Argentina
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither name of the Core SDI S.A. nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * om_snmptrap - Modular Syslog 1.xx module used to generate SNMP traps.
 * Author: Hernan Gips <chipi@corest.com>
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "../../config.h"
#include "../../modules.h"
#include "../../syslogd.h"

#include <ucd-snmp/asn1.h>
#include <ucd-snmp/mib.h>
#include <ucd-snmp/parse.h>
#include <ucd-snmp/ucd-snmp-config.h>
#include <ucd-snmp/ucd-snmp-includes.h>
#include <ucd-snmp/system.h>

#ifndef DEFAULT_HOST
#define DEFAULT_HOST		"127.0.0.1"
#endif

#ifndef DEFAULT_PORT
#define DEFAULT_PORT		SNMP_TRAP_PORT
#endif

#ifndef DEFAULT_COMMUNITY
#define DEFAULT_COMMUNITY	"private"
#endif

#ifndef DEFAULT_GENERIC_TRAP
#define DEFAULT_GENERIC_TRAP	SNMP_TRAP_ENTERPRISESPECIFIC
#endif

#ifndef DEFAULT_SPECIFIC_TRAP
#define DEFAULT_SPECIFIC_TRAP	0
#endif


extern char *__progname;


struct om_snmptrap_ctx {
	char	 host[MAXHOSTNAMELEN];
	uint16_t port;
	char	 community[COMMUNITY_MAX_LEN];
	oid	 trap_oid[MAX_OID_LEN];
	size_t	 trap_oid_len;
	int	 generic_type;
	int	 specific_type;
	oid	 var_oid[MAX_OID_LEN];
	size_t	 var_oid_len;;
};


/*
 * om_snmptrap_init:
 *	%snmptrap [-h host] [-p port] [-c community] -t trap_oid
 *		[-g generic_type] [-s specific_type] [var_oid]
 *
 *	host		Trap server (default 127.0.0.1)
 *	port		Trap port (default 162)
 *	community	Community (default 'private')
 *	trap_oid	The trap oid
 *	generic_type	The generic trap type (default 6: ENTERPRISE SPECIFIC)
 *	specific_type	The specific trap type (default: 0)
 *	var_oid		An extra variable sent with the trap. Its content
 *			will be the syslog message.
 */
int
om_snmptrap_init(int argc, char **argv, struct filed *f, char *prog, void **ctx,
		 char **status)
{
	extern int optreset;	/* Present on some BSD based systems */
	extern int optind;
	struct om_snmptrap_ctx *c;
	unsigned char statbuf[256], *buf;
	size_t buflen, bufolen;
	int ch;

	dprintf(MSYSLOG_INFORMATIVE, "om_snmptrap_init: Entering, "
	    "callded by %s.\n", prog);

	/* By the doubts :) */
	if (argv == NULL || *argv == NULL || argc == 0 || f == NULL ||
	    ctx == NULL) {
		errno = EINVAL;
		return (-1);
	}

	/* Create a temporal buffer */
	buflen = 256;
	buf = (unsigned char *)malloc(buflen * sizeof(unsigned char));
	if (buf == NULL) {
		dprintf(MSYSLOG_CRITICAL, "om_snmptrap_init: %s.\n",
		    strerror(errno));
		return (-1);
	}

	/* Create the snmptrap context */
	c = (struct om_snmptrap_ctx *)malloc(sizeof(struct om_snmptrap_ctx));
	if (c == NULL) {
		dprintf(MSYSLOG_CRITICAL, "om_snmptrap_init: "
		    "Can't create context: %s.\n", strerror(errno));
		free(buf);
		return (-1);
	}

	/*
	 * Initialize snmp related functions
	 * see mib_api(3) and snmp_agent_api(3)
	 */
	init_mib();
	read_all_mibs();
	init_snmp(__progname);

	/* Set default context values */
	strlcpy(c->host, DEFAULT_HOST, sizeof(c->host));
	c->port = DEFAULT_PORT;
	strlcpy(c->community, DEFAULT_COMMUNITY, sizeof(c->community));
	c->trap_oid_len = 0;
	c->generic_type = DEFAULT_GENERIC_TRAP;
	c->specific_type = DEFAULT_SPECIFIC_TRAP;
	c->var_oid_len = 0;

	/* Parse the command line */
	optreset = 1;		/* Present on some BSD based systems */
	optind = 1;
	while ((ch = getopt(argc, argv, "c:g:h:p:s:t:")) != -1)
		switch (ch) {
		case 'c': /* Community */
			if (strcmp(optarg, "private") && strcmp(optarg,
			    "public"))
				dprintf(MSYSLOG_WARNING, "om_snmptrap_init: "
				    "Invalid -c value '%s'. Ignoring, using "
				    "default.\n", optarg);
			else 
				strlcpy(c->community, optarg,
				    sizeof(c->community));
			break;

		case 'g': /* Generic Trap Type */
			c->generic_type =
			    (unsigned char)strtoul(optarg, NULL, 10);
			break;

		case 'h': /* Host */
			strlcpy(c->host, optarg, sizeof(c->host));
			break;

		case 'p': /* Port */
			c->port = (uint16_t)strtoul(optarg, NULL, 10);
			break;

		case 's': /* Specific Trap Type */
			c->specific_type =
			    (unsigned char)strtoul(optarg, NULL, 10);
			break;

		case 't': /* Trap Object Identifier */
			c->trap_oid_len = MAX_OID_LEN;
			if (snmp_parse_oid(optarg, c->trap_oid,
			    &c->trap_oid_len) == NULL) {
				dprintf(MSYSLOG_CRITICAL, "om_snmptrap_init: "
				    "Can't the parse the trap oid: %s: %s.\n",
				    optarg, snmp_api_errstring(snmp_errno));
				goto fail;
			}
			break;

		default:
			dprintf(MSYSLOG_WARNING, "om_snmptrap_init: Invalid "
			    "command line option: -%c. Ignoring.\n", ch);
		}


	/* The Trap Object Identifier is mandatory (-t option) */
	if (c->trap_oid_len == 0) {
		dprintf(MSYSLOG_CRITICAL, "om_snmptrap_init: "
		    "Missing trap object identifier (option -t).\n"); 
		goto fail;
	}

	/* Get optional list of variables */
	if (argc - optind) {
		c->var_oid_len = MAX_OID_LEN;
		if (snmp_parse_oid(argv[optind], c->var_oid, &c->var_oid_len)
		    == NULL) {
			dprintf(MSYSLOG_CRITICAL, "om_snmptrap_init: "
			    "Can't parse variable oid: %s: %s.\n",
			    argv[argc - optind - 1],
			    snmp_api_errstring(snmp_errno));
			goto fail;
		}

		optind++;
	}

	if (argc - optind)
		dprintf(MSYSLOG_WARNING, "om_snmptrap_init: Extra invalid "
		    "arguments. Ignored.\n");
	
	bufolen = 0;
	sprint_realloc_objid(&buf, &buflen, &bufolen, 1, c->trap_oid,
	    c->trap_oid_len);

	snprintf(statbuf, sizeof(statbuf), "om_snmptrap: host=%s, port=%d, "
	    "community=%s, trap_oid=%s, generic_type=%d, specific_type=%d\n",
	    c->host, c->port, c->community, buf, c->generic_type,
	    c->specific_type);

	*ctx = (void *)c;
	*status = strdup(statbuf);
	free(buf);
	return (1);

fail:
	free(c);
	free(buf);
	return (-1);
}


int
om_snmptrap_close(struct filed *f, void *ctx)
{
	return (0);
}


int
om_snmptrap_write(struct filed *f, int flags, struct m_msg *msg, void *ctx)
{
	struct snmp_session session, *ss;
	struct sockaddr_in *agent_addr;
	struct snmp_pdu *pdu;
	struct om_snmptrap_ctx *c;

	c = (struct om_snmptrap_ctx *)ctx;

	/* Identify an SNMP session */
	snmp_sess_init(&session);
	session.version = SNMP_VERSION_1;
	session.peername = c->host;
	session.remote_port = c->port;
	session.community = c->community;
	session.community_len = strlen(session.community);

	/* Open the SNMP session */
	ss = snmp_open(&session);
	if (ss == NULL) {
		dprintf(MSYSLOG_CRITICAL, "om_snmptrap_write: Can't create "
		    "session to host %s, port %d: %s.\n", c->host, c->port,
		    snmp_api_errstring(snmp_errno));
		return (-1);
	}

	/* Create a PDU for the TRAP */
	pdu = snmp_pdu_create(SNMP_MSG_TRAP);
	if (pdu == NULL) {
		dprintf(MSYSLOG_CRITICAL, "om_snmptrap_write: Can't create "
		    "pdu: %s.\n", snmp_api_errstring(snmp_errno));
		return (-1);
	}

	pdu->enterprise = (oid *)malloc(sizeof(oid) * c->trap_oid_len);
	if (pdu->enterprise == NULL) {
		dprintf(MSYSLOG_CRITICAL, "om_snmptrap_write: %s.\n",
		    strerror(errno));
		snmp_free_pdu(pdu);
		return (-1);
	}

	memcpy(pdu->enterprise, c->trap_oid, sizeof(oid) * c->trap_oid_len);
	pdu->enterprise_length = c->trap_oid_len;
	pdu->trap_type = c->generic_type;
	pdu->specific_type = c->specific_type;
	pdu->time = get_uptime();
	agent_addr = (struct sockaddr_in *)&pdu->agent_addr;
	agent_addr->sin_family = AF_INET;
	agent_addr->sin_addr.s_addr  = get_myaddr();

	/* Add the optional variable to the PDU */
	if (c->var_oid_len)
		if (snmp_add_var(pdu, c->var_oid, c->var_oid_len, 's',
		    msg->msg) != 0)
			dprintf(MSYSLOG_CRITICAL, "om_snmptrap_write: "
			    "Can't add variable to the pdu, trap will be"
			    "sent without it to host %s, port %s: %s.\n",
			    c->host, c->port, snmp_api_errstring(snmp_errno));

	if (snmp_send(ss, pdu) == 0) {
		dprintf(MSYSLOG_CRITICAL, "om_snmptrap_write: "
		    "Can't send trap to host %s, port %d: %s.\n", c->host,
		    c->port, snmp_api_errstring(snmp_errno));
		snmp_free_pdu(pdu);
		snmp_close(ss);
		return (-1);
	}

	dprintf(MSYSLOG_INFORMATIVE, "om_snmptrap_write: "
	    "Trap sent to host %s, port %d.\n", c->host, c->port);

	/* The pdu is freed by the library (see snmp_send(3)) */
	snmp_close(ss);		
	return (1);
}

