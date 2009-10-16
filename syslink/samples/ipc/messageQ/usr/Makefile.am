#
#  Copyright 2001-2009 Texas Instruments - http://www.ti.com/
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

#   ============================================================================
#   @file   Makefile.am
#
#   @brief  Makefile for user-side MessageQ samples
#
#   ============================================================================

PROJROOT=../../..
INCLUDE = \
	-I $(PROJROOT)/../api/include \
	-I $(PROJROOT)/inc


AM_CFLAGS = -Wall -g -O2 $(INCLUDE)-finline-functions -D$(PROCFAMILY)
AM_CFLAGS += -DSYSLINK_TRACE_ENABLE
AM_CFLAGS += -DSYSLINK_USE_SYSMGR
#TO enable loader uncomment the below flag.
#CFLAGS += -DSYSLINK_USE_LOADER

LDPATH=$(PROJROOT)/../api/src

API_LIBS = \
	$(LDPATH)/utils/libutils.la \
	$(LDPATH)/notify/libnotify.la \
	$(LDPATH)/ipc/libipc.la \
	$(LDPATH)/procmgr/libprocmgr.la \
	$(LDPATH)/sysmgr/libsysmgr.la \
	$(LDPATH)/sysmemmgr/libsysmemmgr.la


bin_PROGRAMS = messageQApp.out messageQApp1.out

messageQApp_out_SOURCES = \
	MessageQAppOS.c \
	MessageQApp.c

messageQApp1_out_SOURCES = \
	MessageQAppOS1.c \
	MessageQApp1.c

messageQApp_out_CPPFLAGS = $(AM_CLFAGS)
messageQApp1_out_CPPFLAGS = $(AM_CLFAGS)

messageQApp_out_LDADD = $(API_LIBS)

messageQApp1_out_LDADD = $(API_LIBS)
