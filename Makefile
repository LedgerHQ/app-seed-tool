#*******************************************************************************
#   Ledger Seed Tool application
#   (c) 2016-2026 Ledger SAS
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#*******************************************************************************

ifeq ($(BOLOS_SDK),)
$(error Environment variable BOLOS_SDK is not set)
endif

include $(BOLOS_SDK)/Makefile.target

APPNAME = "Seed Tool"
APPVERSION_M = 1
APPVERSION_N = 9
APPVERSION_P = 0
APPVERSION   = "$(APPVERSION_M).$(APPVERSION_N).$(APPVERSION_P)"

APPVERSION_RC = 0
ifneq ($(APPVERSION_RC), 0)
    APPVERSION := $(APPVERSION)-rc.$(APPVERSION_RC)
endif

VARIANT_PARAM  = NONE
VARIANT_VALUES = seed_tool

CURVE_APP_LOAD_PARAMS = secp256k1
PATH_APP_LOAD_PARAMS = ""
HAVE_APPLICATION_FLAG_DERIVE_MASTER = 1

ICON_NANOS  = icons/icon_seed_16px.gif
ICON_NANOSP = icons/icon_seed_14px.png
ICON_NANOX  = icons/icon_seed_14px.png
ICON_STAX   = icons/icon_seed_32px.png
ICON_FLEX   = icons/icon_seed_40px.png
ICON_APEX_P = icons/icon_seed_32px.png

#DEFINES += HAVE_ELECTRUM

# Whole-app stack canary written at init and checked on every I/O event loop
# iteration on nanos-secure-sdk (io_seproxyhal_se_reset() on mismatch); write
# path only, no observable app-side check, on newer SDKs (nanox/stax/flex/...).
DEFINES += HAVE_BOLOS_APP_STACK_CANARY

ifeq ($(TARGET_NAME),TARGET_NANOS)
    $(info Using BAGL)
    DISABLE_STANDARD_USB = 1
else ifeq ($(TARGET_NAME), $(filter $(TARGET_NAME), TARGET_NANOS2 TARGET_NANOX))
    $(info Using BAGL)
else
    $(info Using NBGL)
    ENABLE_NBGL_KEYBOARD = 1
    ENABLE_NBGL_KEYPAD = 1
endif

# Per-function stack canary (-fstack-protector-strong). Ignored by the nanos
# SDK, which has no stack protector support.
ENABLE_STACK_PROTECTOR = 1

DEBUG = 0

APP_SOURCE_PATH += src

include $(BOLOS_SDK)/Makefile.standard_app

ifeq ($(TARGET_NAME),TARGET_NANOS)
    # Appended after the SDK makefiles so that they win over the SDK defaults:
    # -gdwarf-4 has to follow the -g0 that Makefile.defines adds on a release
    # build, and -Wno-unterminated-string-initialization has to follow the
    # -Wextra that would otherwise turn the warning back on.
    CFLAGS += -gdwarf-4 -Wno-unterminated-string-initialization
    APP_LOAD_PARAMS += --apiLevel 0
endif
