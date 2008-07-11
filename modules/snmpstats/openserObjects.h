/*
 * $Id$
 *
 * SNMPStats Module 
 * Copyright (C) 2006 SOMA Networks, INC.
 * Written by: Jeffrey Magder (jmagder@somanetworks.com)
 *
 * This file is part of opensips, a free SIP server.
 *
 * opensips is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * opensips is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 * History:
 * --------
 * 2006-11-23 initial version (jmagder)
 * 
 * Note: this file originally auto-generated by mib2c using
 *        : mib2c.scalar.conf,v 1.9 2005/01/07 09:37:18 dts12 Exp $
 *
 * This file defines all registration and handling of all scalars defined in the
 * OPENSER-MIB.  Please see OPENSER-MIB for the complete descriptions of the
 * individual scalars.
 *
 */

#ifndef OPENSEROBJECTS_H
#define OPENSEROBJECTS_H

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

/* function declarations */
void init_openserObjects(void);

Netsnmp_Node_Handler handle_openserMsgQueueDepth;
Netsnmp_Node_Handler handle_openserMsgQueueMinorThreshold;
Netsnmp_Node_Handler handle_openserMsgQueueMajorThreshold;
Netsnmp_Node_Handler handle_openserMsgQueueDepthAlarmStatus;
Netsnmp_Node_Handler handle_openserMsgQueueDepthMinorAlarm;
Netsnmp_Node_Handler handle_openserMsgQueueDepthMajorAlarm;
Netsnmp_Node_Handler handle_openserCurNumDialogs;
Netsnmp_Node_Handler handle_openserCurNumDialogsInProgress;
Netsnmp_Node_Handler handle_openserCurNumDialogsInSetup;
Netsnmp_Node_Handler handle_openserTotalNumFailedDialogSetups;
Netsnmp_Node_Handler handle_openserDialogLimitMinorThreshold;
Netsnmp_Node_Handler handle_openserDialogLimitMajorThreshold;
Netsnmp_Node_Handler handle_openserDialogUsageState;
Netsnmp_Node_Handler handle_openserDialogLimitAlarmStatus;
Netsnmp_Node_Handler handle_openserDialogLimitMinorAlarm;
Netsnmp_Node_Handler handle_openserDialogLimitMajorAlarm;

/* The following four functions are used to retrieve thresholds set via the
 * opensips.cfg configuration file. */
int get_msg_queue_minor_threshold();
int get_msg_queue_major_threshold();
int get_dialog_minor_threshold();
int get_dialog_major_threshold();

#endif /* OPENSEROBJECTS_H */
