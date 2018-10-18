# Compiler flags...
CPP_COMPILER = g++
C_COMPILER = gcc

# Include paths...
#Debug_Include_Path=-I"/usr/include/postgresql/"
#Release_Include_Path=-I"/usr/include/postgresql/"

# Library paths...
Debug_Library_Path=-L"/usr/lib/"  -L"/usr/local/lib/"
Release_Library_Path=-L"/usr/lib/"  -L"/usr/local/lib/"

# Additional libraries...
Debug_Libraries=-Wl,--start-group -lpq -lssl  -lcrypto  -lpng -lqrencode -lpthread   -lz -Wl,--end-group

# I think that need remove png and qrencode libs. make they separate project. and connect them.
# -l:libboost_system.a     -- header only used.
#  -l:libboost_thread.a   -- std::thread used
# -l:libboost_program_options.a  -- my velocy 
# -l:libjsoncpp.a
# -lcurl -ljsonrpccpp-common -ljsonrpccpp-client  -- used boost asio instead of they.
# jsonrpccpp-common  jsonrpccpp-client and curl dependencies   removed  !!!
# -l:libboost_date_time.a -l:libboost_filesystem.a  -- not used never or used linux functions.
# 

Release_Libraries=$(Debug_Libraries)

# Preprocessor definitions...
Debug_Preprocessor_Definitions=-D GCC_BUILD 
Release_Preprocessor_Definitions=-D GCC_BUILD  -D NDEBUG

# Implictly linked object files...
Debug_Implicitly_Linked_Objects=
Release_Implicitly_Linked_Objects=

# Compiler flags...
Debug_Compiler_Flags=-O0 -g -std=c++11 -Wall -pedantic 
Release_Compiler_Flags=-O2  -Os -std=c++11 -Wall  -flto

# Builds all configurations for this project...
.PHONY: build_all_configurations
#build_all_configurations: Debug Release
build_all_configurations: Release

TOP_DEBUG_BUILD = ../build/x64/gccDebug
DEBUGOBJ = $(TOP_DEBUG_BUILD)/osond.o      $(TOP_DEBUG_BUILD)/osoninit.o       \
$(TOP_DEBUG_BUILD)/clientapi.o             $(TOP_DEBUG_BUILD)/admin.o          \
$(TOP_DEBUG_BUILD)/utils_endian.o          $(TOP_DEBUG_BUILD)/DB_T.o           \
$(TOP_DEBUG_BUILD)/log.o                   $(TOP_DEBUG_BUILD)/users.o          \
$(TOP_DEBUG_BUILD)/cards.o                 $(TOP_DEBUG_BUILD)/transaction.o    \
$(TOP_DEBUG_BUILD)/sms_sender.o            $(TOP_DEBUG_BUILD)/adminapi.o       \
$(TOP_DEBUG_BUILD)/Merchant_T.o            $(TOP_DEBUG_BUILD)/purchase.o       \
$(TOP_DEBUG_BUILD)/news.o                  $(TOP_DEBUG_BUILD)/png_image.o      \
$(TOP_DEBUG_BUILD)/utils.o                 $(TOP_DEBUG_BUILD)/topupmerchant.o          \
$(TOP_DEBUG_BUILD)/eocp_api.o              $(TOP_DEBUG_BUILD)/merchant_api.o   \
$(TOP_DEBUG_BUILD)/Pusher.o                $(TOP_DEBUG_BUILD)/queue.o          \
$(TOP_DEBUG_BUILD)/fault.o                 $(TOP_DEBUG_BUILD)/periodic_bill.o  \
$(TOP_DEBUG_BUILD)/bills.o                 $(TOP_DEBUG_BUILD)/msg_android.o    \
$(TOP_DEBUG_BUILD)/eopc_queue.o            $(TOP_DEBUG_BUILD)/bank.o           \
$(TOP_DEBUG_BUILD)/http_request.o          $(TOP_DEBUG_BUILD)/ssl_server.o     \
$(TOP_DEBUG_BUILD)/paynet_gate.o           $(TOP_DEBUG_BUILD)/application.o    \
$(TOP_DEBUG_BUILD)/http_server.o           $(TOP_DEBUG_BUILD)/icons.o


# Builds the Debug configuration...
.PHONY: Debug
Debug: create_folders $(DEBUGOBJ)
	$(CPP_COMPILER) $(DEBUGOBJ) $(Debug_Library_Path) $(Debug_Libraries) -Wl,-rpath,./ -o ../bin/debug/osond

-include $(TOP_DEBUG_BUILD)/ssl_server.d
$(TOP_DEBUG_BUILD)/ssl_server.o: ssl_server.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c $< $(Debug_Include_Path) -o $@
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM $< $(Debug_Include_Path) > $(@D)/$(*F).d

-include $(TOP_DEBUG_BUILD)/osond.d
$(TOP_DEBUG_BUILD)/osond.o: osond.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c $< $(Debug_Include_Path) -o $@
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM $< $(Debug_Include_Path) > $(@D)/$(*F).d

-include $(TOP_DEBUG_BUILD)/utils.d
$(TOP_DEBUG_BUILD)/utils.o: utils.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c $< $(Debug_Include_Path) -o $@
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM $< $(Debug_Include_Path) > $(@D)/$(*F).d

-include $(TOP_DEBUG_BUILD)/osoninit.d
$(TOP_DEBUG_BUILD)/osoninit.o: osoninit.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c $< $(Debug_Include_Path) -o $@
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM $< $(Debug_Include_Path) > $(@D)/$(*F).d

-include $(TOP_DEBUG_BUILD)/adminapi.d
$(TOP_DEBUG_BUILD)/adminapi.o: adminapi.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c $< $(Debug_Include_Path) -o $@
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM $< $(Debug_Include_Path) > $(@D)/$(*F).d

-include $(TOP_DEBUG_BUILD)/admin.d
$(TOP_DEBUG_BUILD)/admin.o: admin.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c $< $(Debug_Include_Path) -o $@
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM $< $(Debug_Include_Path) > $(@D)/$(*F).d

-include $(TOP_DEBUG_BUILD)/clientapi.d
$(TOP_DEBUG_BUILD)/clientapi.o: clientapi.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c $< $(Debug_Include_Path) -o $@
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM $< $(Debug_Include_Path) > $(@D)/$(*F).d

-include $(TOP_DEBUG_BUILD)/utils_endian.d
$(TOP_DEBUG_BUILD)/utils_endian.o: utils_endian.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c $< $(Debug_Include_Path) -o $@
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM $< $(Debug_Include_Path) > $(@D)/$(*F).d

-include $(TOP_DEBUG_BUILD)/DB_T.d
$(TOP_DEBUG_BUILD)/DB_T.o: DB_T.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c $< $(Debug_Include_Path) -o $@
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM $< $(Debug_Include_Path) > $(@D)/$(*F).d

-include $(TOP_DEBUG_BUILD)/log.d
$(TOP_DEBUG_BUILD)/log.o: log.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c $< $(Debug_Include_Path) -o $@
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM $< $(Debug_Include_Path) > $(@D)/$(*F).d

-include $(TOP_DEBUG_BUILD)/users.d
$(TOP_DEBUG_BUILD)/users.o: users.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c $< $(Debug_Include_Path) -o $@
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM $< $(Debug_Include_Path) > $(@D)/$(*F).d

-include $(TOP_DEBUG_BUILD)/cards.d
$(TOP_DEBUG_BUILD)/cards.o: cards.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c $< $(Debug_Include_Path) -o $@
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM $< $(Debug_Include_Path) > $(@D)/$(*F).d

-include $(TOP_DEBUG_BUILD)/transaction.d
$(TOP_DEBUG_BUILD)/transaction.o: transaction.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c $< $(Debug_Include_Path) -o $@
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM $< $(Debug_Include_Path) > $(@D)/$(*F).d

-include $(TOP_DEBUG_BUILD)/Merchant_T.d
$(TOP_DEBUG_BUILD)/Merchant_T.o: Merchant_T.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c $< $(Debug_Include_Path) -o $@
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM $< $(Debug_Include_Path) > $(@D)/$(*F).d

-include $(TOP_DEBUG_BUILD)/purchase.d
$(TOP_DEBUG_BUILD)/purchase.o: purchase.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c $< $(Debug_Include_Path) -o $@
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM $< $(Debug_Include_Path) > $(@D)/$(*F).d

-include $(TOP_DEBUG_BUILD)/sms_sender.d
$(TOP_DEBUG_BUILD)/sms_sender.o: sms_sender.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c $< $(Debug_Include_Path) -o $@
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM $< $(Debug_Include_Path) > $(@D)/$(*F).d

-include $(TOP_DEBUG_BUILD)/news.d
$(TOP_DEBUG_BUILD)/news.o: news.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c $< $(Debug_Include_Path) -o $@
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM $< $(Debug_Include_Path) > $(@D)/$(*F).d

-include $(TOP_DEBUG_BUILD)/png_image.d
$(TOP_DEBUG_BUILD)/png_image.o: png_image.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c $< $(Debug_Include_Path) -o $@
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM $< $(Debug_Include_Path) > $(@D)/$(*F).d

-include $(TOP_DEBUG_BUILD)/topupmerchant.d
$(TOP_DEBUG_BUILD)/topupmerchant.o: topupmerchant.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c $< $(Debug_Include_Path) -o $@
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM $< $(Debug_Include_Path) > $(@D)/$(*F).d

-include $(TOP_DEBUG_BUILD)/eocp_api.d
$(TOP_DEBUG_BUILD)/eocp_api.o: eocp_api.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c $< $(Debug_Include_Path) -o $@
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM $< $(Debug_Include_Path) > $(@D)/$(*F).d

-include $(TOP_DEBUG_BUILD)/merchant_api.d
$(TOP_DEBUG_BUILD)/merchant_api.o: merchant_api.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c $< $(Debug_Include_Path) -o $@
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM $< $(Debug_Include_Path) > $(@D)/$(*F).d

-include $(TOP_DEBUG_BUILD)/Pusher.d
$(TOP_DEBUG_BUILD)/Pusher.o: Pusher.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c $< $(Debug_Include_Path) -o $@
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM $< $(Debug_Include_Path) > $(@D)/$(*F).d

-include $(TOP_DEBUG_BUILD)/queue.d
$(TOP_DEBUG_BUILD)/queue.o: queue.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c $< $(Debug_Include_Path) -o $@
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM $< $(Debug_Include_Path) > $(@D)/$(*F).d

-include $(TOP_DEBUG_BUILD)/fault.d
$(TOP_DEBUG_BUILD)/fault.o: fault.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c $< $(Debug_Include_Path) -o $@
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM $< $(Debug_Include_Path) > $(@D)/$(*F).d

-include $(TOP_DEBUG_BUILD)/eopc_queue.d
$(TOP_DEBUG_BUILD)/eopc_queue.o: eopc_queue.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c $< $(Debug_Include_Path) -o $@
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM $< $(Debug_Include_Path) > $(@D)/$(*F).d

-include $(TOP_DEBUG_BUILD)/periodic_bill.d
$(TOP_DEBUG_BUILD)/periodic_bill.o: periodic_bill.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c $< $(Debug_Include_Path) -o $@
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM $< $(Debug_Include_Path) > $(@D)/$(*F).d

-include $(TOP_DEBUG_BUILD)/bills.d
$(TOP_DEBUG_BUILD)/bills.o: bills.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c $< $(Debug_Include_Path) -o $@
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM $< $(Debug_Include_Path) > $(@D)/$(*F).d

-include $(TOP_DEBUG_BUILD)/msg_android.d
$(TOP_DEBUG_BUILD)/msg_android.o: msg_android.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c $< $(Debug_Include_Path) -o $@
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM $< $(Debug_Include_Path) > $(@D)/$(*F).d

-include $(TOP_DEBUG_BUILD)/bank.d
$(TOP_DEBUG_BUILD)/bank.o: bank.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c $< $(Debug_Include_Path) -o $@
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM $< $(Debug_Include_Path) > $(@D)/$(*F).d

-include $(TOP_DEBUG_BUILD)/http_request.d
$(TOP_DEBUG_BUILD)/http_request.o: http_request.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c $< $(Debug_Include_Path) -o $@
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM $< $(Debug_Include_Path) > $(@D)/$(*F).d

-include $(TOP_DEBUG_BUILD)/paynet_gate.d
$(TOP_DEBUG_BUILD)/paynet_gate.o: paynet_gate.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c $< $(Debug_Include_Path) -o $@
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM $< $(Debug_Include_Path) > $(@D)/$(*F).d

-include $(TOP_DEBUG_BUILD)/application.d
$(TOP_DEBUG_BUILD)/application.o: application.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c $< $(Debug_Include_Path) -o $@
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM $< $(Debug_Include_Path) > $(@D)/$(*F).d

-include $(TOP_DEBUG_BUILD)/http_server.d
$(TOP_DEBUG_BUILD)/http_server.o: http_server.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c $< $(Debug_Include_Path) -o $@
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM $< $(Debug_Include_Path) > $(@D)/$(*F).d

-include $(TOP_DEBUG_BUILD)/icons.d
$(TOP_DEBUG_BUILD)/icons.o: icons.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c $< $(Debug_Include_Path) -o $@
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM $< $(Debug_Include_Path) > $(@D)/$(*F).d

################################################################################
#				Release
################################################################################


TOP_RELEASE_BUILD = ../build/x64/gccRelease
RELEASEOBJ = $(TOP_RELEASE_BUILD)/osond.o $(TOP_RELEASE_BUILD)/osoninit.o      \
$(TOP_RELEASE_BUILD)/clientapi.o          $(TOP_RELEASE_BUILD)/admin.o         \
$(TOP_RELEASE_BUILD)/utils_endian.o       $(TOP_RELEASE_BUILD)/DB_T.o          \
$(TOP_RELEASE_BUILD)/log.o                $(TOP_RELEASE_BUILD)/users.o         \
$(TOP_RELEASE_BUILD)/cards.o              $(TOP_RELEASE_BUILD)/transaction.o   \
$(TOP_RELEASE_BUILD)/sms_sender.o         $(TOP_RELEASE_BUILD)/adminapi.o      \
$(TOP_RELEASE_BUILD)/Merchant_T.o         $(TOP_RELEASE_BUILD)/purchase.o      \
$(TOP_RELEASE_BUILD)/news.o               $(TOP_RELEASE_BUILD)/png_image.o     \
$(TOP_RELEASE_BUILD)/utils.o              $(TOP_RELEASE_BUILD)/topupmerchant.o \
$(TOP_RELEASE_BUILD)/eocp_api.o           $(TOP_RELEASE_BUILD)/merchant_api.o  \
$(TOP_RELEASE_BUILD)/Pusher.o             $(TOP_RELEASE_BUILD)/queue.o         \
$(TOP_RELEASE_BUILD)/fault.o              $(TOP_RELEASE_BUILD)/periodic_bill.o \
$(TOP_RELEASE_BUILD)/bills.o              $(TOP_RELEASE_BUILD)/msg_android.o   \
$(TOP_RELEASE_BUILD)/eopc_queue.o         $(TOP_RELEASE_BUILD)/bank.o          \
$(TOP_RELEASE_BUILD)/http_request.o       $(TOP_RELEASE_BUILD)/ssl_server.o    \
$(TOP_RELEASE_BUILD)/paynet_gate.o        $(TOP_RELEASE_BUILD)/application.o   \
$(TOP_RELEASE_BUILD)/http_server.o        $(TOP_RELEASE_BUILD)/icons.o  


# Builds the Debug configuration...
.PHONY: Release
Release: create_folders $(RELEASEOBJ)
	$(CPP_COMPILER) $(RELEASEOBJ) $(Release_Library_Path) $(Release_Libraries) -Wl,-rpath,./ -o ../bin/release/osond

-include $(TOP_RELEASE_BUILD)/ssl_server.d
$(TOP_RELEASE_BUILD)/ssl_server.o: ssl_server.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c $< $(Release_Include_Path) -o $@
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM $< $(Release_Include_Path) > $(@D)/$(*F).d


-include $(TOP_RELEASE_BUILD)/osond.d
$(TOP_RELEASE_BUILD)/osond.o: osond.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c $< $(Release_Include_Path) -o $@
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM $< $(Release_Include_Path) > $(@D)/$(*F).d

-include $(TOP_RELEASE_BUILD)/utils.d
$(TOP_RELEASE_BUILD)/utils.o: utils.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c $< $(Release_Include_Path) -o $@
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM $< $(Release_Include_Path) > $(@D)/$(*F).d

-include $(TOP_RELEASE_BUILD)/osoninit.d
$(TOP_RELEASE_BUILD)/osoninit.o: osoninit.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c $< $(Release_Include_Path) -o $@
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM $< $(Release_Include_Path) > $(@D)/$(*F).d

-include $(TOP_RELEASE_BUILD)/adminapi.d
$(TOP_RELEASE_BUILD)/adminapi.o: adminapi.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c $< $(Release_Include_Path) -o $@
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM $< $(Release_Include_Path) > $(@D)/$(*F).d

-include $(TOP_RELEASE_BUILD)/admin.d
$(TOP_RELEASE_BUILD)/admin.o: admin.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c $< $(Release_Include_Path) -o $@
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM $< $(Release_Include_Path) > $(@D)/$(*F).d

-include $(TOP_RELEASE_BUILD)/clientapi.d
$(TOP_RELEASE_BUILD)/clientapi.o: clientapi.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c $< $(Release_Include_Path) -o $@
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM $< $(Release_Include_Path) > $(@D)/$(*F).d

-include $(TOP_RELEASE_BUILD)/utils_endian.d
$(TOP_RELEASE_BUILD)/utils_endian.o: utils_endian.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c $< $(Release_Include_Path) -o $@
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM $< $(Release_Include_Path) > $(@D)/$(*F).d

-include $(TOP_RELEASE_BUILD)/DB_T.d
$(TOP_RELEASE_BUILD)/DB_T.o: DB_T.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c $< $(Release_Include_Path) -o $@
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM $< $(Release_Include_Path) > $(@D)/$(*F).d

-include $(TOP_RELEASE_BUILD)/log.d
$(TOP_RELEASE_BUILD)/log.o: log.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c $< $(Release_Include_Path) -o $@
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM $< $(Release_Include_Path) > $(@D)/$(*F).d

-include $(TOP_RELEASE_BUILD)/users.d
$(TOP_RELEASE_BUILD)/users.o: users.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c $< $(Release_Include_Path) -o $@
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM $< $(Release_Include_Path) > $(@D)/$(*F).d

-include $(TOP_RELEASE_BUILD)/cards.d
$(TOP_RELEASE_BUILD)/cards.o: cards.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c $< $(Release_Include_Path) -o $@
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM $< $(Release_Include_Path) > $(@D)/$(*F).d

-include $(TOP_RELEASE_BUILD)/transaction.d
$(TOP_RELEASE_BUILD)/transaction.o: transaction.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c $< $(Release_Include_Path) -o $@
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM $< $(Release_Include_Path) > $(@D)/$(*F).d

-include $(TOP_RELEASE_BUILD)/Merchant_T.d
$(TOP_RELEASE_BUILD)/Merchant_T.o: Merchant_T.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c $< $(Release_Include_Path) -o $@
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM $< $(Release_Include_Path) > $(@D)/$(*F).d

-include $(TOP_RELEASE_BUILD)/purchase.d
$(TOP_RELEASE_BUILD)/purchase.o: purchase.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c $< $(Release_Include_Path) -o $@
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM $< $(Release_Include_Path) > $(@D)/$(*F).d

-include $(TOP_RELEASE_BUILD)/sms_sender.d
$(TOP_RELEASE_BUILD)/sms_sender.o: sms_sender.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c $< $(Release_Include_Path) -o $@
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM $< $(Release_Include_Path) > $(@D)/$(*F).d

-include $(TOP_RELEASE_BUILD)/news.d
$(TOP_RELEASE_BUILD)/news.o: news.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c $< $(Release_Include_Path) -o $@
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM $< $(Release_Include_Path) > $(@D)/$(*F).d

-include $(TOP_RELEASE_BUILD)/png_image.d
$(TOP_RELEASE_BUILD)/png_image.o: png_image.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c $< $(Release_Include_Path) -o $@
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM $< $(Release_Include_Path) > $(@D)/$(*F).d

-include $(TOP_RELEASE_BUILD)/topupmerchant.d
$(TOP_RELEASE_BUILD)/topupmerchant.o: topupmerchant.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c $< $(Release_Include_Path) -o $@
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM $< $(Release_Include_Path) > $(@D)/$(*F).d

-include $(TOP_RELEASE_BUILD)/eocp_api.d
$(TOP_RELEASE_BUILD)/eocp_api.o: eocp_api.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c $< $(Release_Include_Path) -o $@
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM $< $(Release_Include_Path) > $(@D)/$(*F).d

-include $(TOP_RELEASE_BUILD)/merchant_api.d
$(TOP_RELEASE_BUILD)/merchant_api.o: merchant_api.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c $< $(Release_Include_Path) -o $@
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM $< $(Release_Include_Path) > $(@D)/$(*F).d

-include $(TOP_RELEASE_BUILD)/Pusher.d
$(TOP_RELEASE_BUILD)/Pusher.o: Pusher.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c $< $(Release_Include_Path) -o $@
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM $< $(Release_Include_Path) > $(@D)/$(*F).d

-include $(TOP_RELEASE_BUILD)/queue.d
$(TOP_RELEASE_BUILD)/queue.o: queue.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c $< $(Release_Include_Path) -o $@
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM $< $(Release_Include_Path) > $(@D)/$(*F).d

-include $(TOP_RELEASE_BUILD)/fault.d
$(TOP_RELEASE_BUILD)/fault.o: fault.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c $< $(Release_Include_Path) -o $@
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM $< $(Release_Include_Path) > $(@D)/$(*F).d

-include $(TOP_RELEASE_BUILD)/eopc_queue.d
$(TOP_RELEASE_BUILD)/eopc_queue.o: eopc_queue.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c $< $(Release_Include_Path) -o $@
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM $< $(Release_Include_Path) > $(@D)/$(*F).d

-include $(TOP_RELEASE_BUILD)/periodic_bill.d
$(TOP_RELEASE_BUILD)/periodic_bill.o: periodic_bill.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c $< $(Release_Include_Path) -o $@
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM $< $(Release_Include_Path) > $(@D)/$(*F).d

-include $(TOP_RELEASE_BUILD)/bills.d
$(TOP_RELEASE_BUILD)/bills.o: bills.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c $< $(Release_Include_Path) -o $@
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM $< $(Release_Include_Path) > $(@D)/$(*F).d

-include $(TOP_RELEASE_BUILD)/msg_android.d
$(TOP_RELEASE_BUILD)/msg_android.o: msg_android.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c $< $(Release_Include_Path) -o $@
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM $< $(Release_Include_Path) > $(@D)/$(*F).d

-include $(TOP_RELEASE_BUILD)/bank.d
$(TOP_RELEASE_BUILD)/bank.o: bank.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c $< $(Release_Include_Path) -o $@
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM $< $(Release_Include_Path) > $(@D)/$(*F).d

-include $(TOP_RELEASE_BUILD)/http_request.d
$(TOP_RELEASE_BUILD)/http_request.o: http_request.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c $< $(Release_Include_Path) -o $@
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM $< $(Release_Include_Path) > $(@D)/$(*F).d

-include $(TOP_RELEASE_BUILD)/paynet_gate.d
$(TOP_RELEASE_BUILD)/paynet_gate.o: paynet_gate.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c $< $(Release_Include_Path) -o $@
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM $< $(Release_Include_Path) > $(@D)/$(*F).d

-include $(TOP_RELEASE_BUILD)/application.d
$(TOP_RELEASE_BUILD)/application.o: application.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c $< $(Release_Include_Path) -o $@
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM $< $(Release_Include_Path) > $(@D)/$(*F).d


-include $(TOP_RELEASE_BUILD)/http_server.d
$(TOP_RELEASE_BUILD)/http_server.o: http_server.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c $< $(Release_Include_Path) -o $@
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM $< $(Release_Include_Path) > $(@D)/$(*F).d

-include $(TOP_RELEASE_BUILD)/icons.d
$(TOP_RELEASE_BUILD)/icons.o: icons.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c $< $(Release_Include_Path) -o $@
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM $< $(Release_Include_Path) > $(@D)/$(*F).d

################################################################################
#				End release
################################################################################


# Creates the intermediate and output folders for each configuration...
.PHONY: create_folders
create_folders:
	mkdir -p ../build
	mkdir -p ../build/x64
	mkdir -p ../build/x64/gccDebug
	mkdir -p ../build/x64
	mkdir -p ../build/x64/gccRelease
	mkdir -p ../bin
	mkdir -p ../bin/debug
	mkdir -p ../bin/release

# Cleans intermediate and output files (objects, libraries, executables)...
.PHONY: clean
clean:
	rm -f ../build/x64/gccDebug/*.o
	rm -f ../build/x64/gccDebug/*.d
	rm -f ../build/x64/gccRelease/*.o
	rm -f ../build/x64/gccRelease/*.d
	rm -f ../bin/debug/*.a
	rm -f ../bin/debug/*.so
	rm -f ../bin/debug/*.dll
	rm -f ../bin/debug/*.exe
	rm -f ../bin/release/*.a
	rm -f ../bin/release/*.so
	rm -f ../bin/release/*.dll
	rm -f ../bin/release/*.exe

