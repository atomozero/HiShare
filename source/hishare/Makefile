# Makefile for building BeShare.  Do plain "make" for a normal build,
# "make debug" for a build with symbols included for debugging, "make clean" to
# force a complete rebuild next time.  Works for BeOS R5 Intel 32 bit CPU, BeOS
# R5 PowerPC 32 bit, Haiku 32 and 64 bit on Intel x86 CPUs.
# Last updated by AGMS20181102

# Use default compilers for C and C++, leave CC and CXX unset (gets you gcc &
# g++), unless needed.
#CC  = gcc
#CXX = gcc

# compilation flags used under any OS or compiler (may be appended to, below)
CXXFLAGS += -I../muscle -Isanta -Isanta/BetterScrollView \
            -Isanta/ColumnListView -Isanta/Cursors -Isanta/NewStrings \
            -Isanta/PrefilledBitmap -Isanta/ScrollViewCorner \
            -I../muscle/zlib/zlib -DMUSCLE_ENABLE_ZLIB_ENCODING

# compilation flags that are specific to the gcc compiler (hard-coded)
GCCFLAGS    = -fno-exceptions -DMUSCLE_NO_EXCEPTIONS -Wall -W -Wno-multichar

# flags to include when compiling the optimized version (with 'make')
CCOPTFLAGS  = -O2

# flags to include when linking (set per operating system, below)
LFLAGS      = 

# libraries to include when linking (set per operating system, below)
LIBS        = 

# names of the executables to compile
EXECUTABLES = HiShare md5test filechopper

# Make uses this to find .cpp files when it's checking dependency datestamps.
VPATH += ../muscle/message ../muscle/besupport ../muscle/iogateway ../muscle/dataio \
         ../muscle/reflector ../muscle/regex ../muscle/system ../muscle/util \
         ../muscle/syslog ../muscle/zlib ../muscle/zlib/zlib santa \
         santa/BetterScrollView santa/ColumnListView santa/Cursors \
         santa/NewStrings santa/PrefilledBitmap santa/ScrollViewCorner

# Various object file and other inputs to the build process.
REGEXOBJS = 
SANTAOBJS = BetterScrollView.o CLVColumn.o CLVColumnLabelView.o CLVEasyItem.o \
            CLVListItem.o ColumnListView.o Cursors.o NewStrings.o \
            PrefilledBitmap.o ScrollViewCorner.o
ZLIBOBJS = adler32.o deflate.o trees.o zutil.o inflate.o inftrees.o inffast.o \
           gzclose.o gzlib.o gzread.o gzwrite.o \
           crc32.o compress.o
RESOURCE = HiShare.rsrc

# If muscled is crashing, try setting this to no; some older compilers can't
# handle memory tracking!  Like GCC2, makes "delete new BMenuItem" fail
# mysteriously, even before anything else has started up right at the beginning
# of main().
MEMORY_TRACKING_SUPPORTED = no

# OSTYPE is "Haiku" or "beos" with that capitalization; if the OS type
# variable is unset, try to set it using the uname shell command.
ifeq ($(OSTYPE),)
  OSTYPE = $(strip $(shell uname))
endif

ifeq ($(OSTYPE),beos)
   ifeq ($(BE_HOST_CPU),ppc)
      CC = mwcc
      CXX = mwcc
      REGEXOBJS = regcomp.o regexec.o regerror.o regfree.o
      VPATH += ../muscle/regex/regex
      CXXFLAGS += -I../muscle/regex/regex -I../muscle/zlib/zlib
      CXXFLAGS += -DMUSCLE_MAX_OUTPUT_CHUNK=1000 -Dstd=""
      CXXFLAGS += $(CCOPTFLAGS)
      LIBS = -ltranslation
      MEMORY_TRACKING_SUPPORTED = no  # mwcc can't handle it correctly :^(
   else # BeOS with Intel x86 CPU.
      CXXFLAGS += $(GCCFLAGS) $(CCOPTFLAGS)
      LIBS = -lbe -lnet -lroot -ltranslation
      ifeq ($(shell ls 2>/dev/null -1 /boot/develop/headers/be/bone/bone_api.h),/boot/develop/headers/be/bone/bone_api.h)
         CXXFLAGS += -I/boot/develop/headers/be/bone -DBONE
         LIBS = -nodefaultlibs -lbind -lsocket -lbe -lroot -L/boot/beos/system/lib
      else # Plain old BeOS R5, not Bone.
        CXXFLAGS += -DMUSCLE_MAX_OUTPUT_CHUNK=1000
      endif
   endif
else # not beos
    ifeq ($(OSTYPE),Haiku)
      CXXFLAGS += $(GCCFLAGS) $(CCOPTFLAGS)
      # _BSD_SOURCE exposes the BSD socket bits PortMapper/muscle need; on 64-bit
      # Haiku muscle must be told the platform is 64-bit or its uintptr/pointer
      # handling truncates (the same flag the HaikuPorts recipe passes).
      CXXFLAGS += -D_BSD_SOURCE
      # MUSCLE 6.11: IPv4-only ip_address (identical to the well-tested Phase-1
      # build).  NOTE: dropping this to enable IPv6 currently breaks connecting
      # to IPv4-only servers on Haiku (the IPv6 socket can't reach an IPv4 dest);
      # real IPv6/dual-stack support needs per-destination socket-family handling
      # and an actual IPv6 network to test on.  See MUSCLE-UPGRADE-PLAN.md.
      CXXFLAGS += -DMUSCLE_AVOID_IPV6
      # 6.11's ThreadLocalStorage needs a threading backend; Haiku has pthreads.
      CXXFLAGS += -DMUSCLE_USE_PTHREADS
      # TLS-encrypted file transfers are disabled in HiShare 1.0 (see BESHARE_TLS_ENABLED
      # in ShareConstants.h), so the OpenSSL dependency is left out by default and the
      # package installs on any Haiku without openssl3.  Build with "make ENABLE_SSL=1"
      # to compile the TLS code back in and link OpenSSL.
      ENABLE_SSL ?= 0
      ifeq ($(ENABLE_SSL),1)
         CXXFLAGS += -DMUSCLE_ENABLE_SSL
         SSL_LIBS = -lssl -lcrypto
         SSL_OBJS = SSLSocketDataIO.o SSLSocketAdapterGateway.o
      endif
      ifeq ($(shell uname -m),x86_64)
         CXXFLAGS += -DMUSCLE_64_BIT_PLATFORM
      endif
      LIBS = -lbe -lnetwork -lbnetapi -lroot -ltranslation -lmedia $(SSL_LIBS)
    else
        ifneq (,$(findstring g++,$(CXX)))
          # not using g++ don't need $GCCFLAGS
          CXXFLAGS += $(CCOPTFLAGS)
        else
          CXXFLAGS += $(GCCFLAGS) $(CCOPTFLAGS)
        endif
    endif
endif

ifeq ($(strip $(MEMORY_TRACKING_SUPPORTED)),yes)
   CXXFLAGS += -DMUSCLE_ENABLE_MEMORY_TRACKING
endif

# Makes all the programs that can be made using just cross-platform code
all : $(EXECUTABLES)

# Sneaky double rule (with a blank line between them - needed) that makes a
# debug build.
debug : CCOPTFLAGS = -g

debug : all

# Left over stuff from BeShare prior to 2.27 or something.  Can't find that .rsrc file.
savebeos : CFLAGS += -DSAVE_BEOS
savebeos : RESOURCE = BeShare-sbe.rsrc
savebeos : all

HiShare : PortMapper.o ReflowingTextView.o ShareNetClient.o ServerConnection.o ShareFileTransfer.o \
          ChatWindow.o ShareApplication.o ShareWindow.o PrivateChatWindow.o \
          MessageTransceiverThread.o Thread.o AcceptSocketsThread.o Message.o \
          SetupSystem.o String.o main.o SysLog.o AbstractMessageIOGateway.o \
          PulseNode.o MessageIOGateway.o PlainTextMessageIOGateway.o \
          PathMatcher.o QueryFilter.o ConvertMessages.o RemoteFileItem.o \
          RemoteUserItem.o RemoteInfo.o ShareColumn.o ShareMIMEInfo.o \
          NetworkUtilityFunctions.o SocketMultiplexer.o ShareUtils.o SplitPane.o md5.o \
          SignalHandlerSession.o SignalMultiplexer.o Directory.o FilePathInfo.o \
          MiscUtilityFunctions.o \
          StringMatcher.o ShareStrings.o ReflectServer.o \
          AbstractReflectSession.o DumbReflectSession.o \
          StorageReflectSession.o GlobalMemoryAllocator.o DataNode.o \
          ServerComponent.o PathMatcher.o StringMatcher.o \
          RateLimitSessionIOPolicy.o ColorPicker.o PirateDemo.o pirate_asm.o \
          ByteBuffer.o ZLibCodec.o ZLibUtilityFunctions.o \
          $(SSL_OBJS) ShareSettingsWindow.o \
          $(SANTAOBJS) $(REGEXOBJS) $(ZLIBOBJS)
	$(CXX) $(LIBS) $(LFLAGS) -o $@ $^ -llocalestub
	xres -o $@ $(RESOURCE)
	# Embed the English (development) message catalog so the Haiku Locale Kit knows our
	# signature+fingerprint; str() bridges to it and falls back to the built-in string
	# tables when no catalog is present.  Non-fatal if the catkeys aren't generated yet
	# (run "make catalogs" first) or the tools are missing.
	-linkcatkeys -tr -o $@ -s "application/x-vnd.HiShare" -l English locale/catalogs/en.catkeys 2>/dev/null || true
	mimeset -F $@

# The intro's chiptune waveform is synthesized in hand-written x86-64 assembly.
pirate_asm.o : pirate_asm.S
	$(CXX) -c -o $@ $<

# Regenerate the per-language .catkeys/.catalog files from ShareStrings.cpp's built-in
# tables (needs python3 + collectcatkeys/linkcatkeys).  The catalogs install under
# data/locale/catalogs/x-vnd.HiShare/ for the Haiku Locale Kit to pick up.
catalogs :
	python3 locale/gen-catkeys.py

md5test : md5.o md5test.o
	$(CXX) $(LIBS) $(LFLAGS) -o $@ $^

filechopper : filechopper.o
	$(CXX) $(LIBS) $(LFLAGS) -o $@ $^

clean :
	rm -fv *.o *.xSYM $(EXECUTABLES)
