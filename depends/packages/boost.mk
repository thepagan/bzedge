package=boost
$(package)_version=1_74_0
$(package)_download_path=https://miodrag.zone/depends/boost
$(package)_file_name=boost_$($(package)_version).tar.bz2
$(package)_sha256_hash=83bfc1507731a0906e387fc28b7ef5417d591429e51e788417fe9ff025e116b1
$(package)_dependencies=native_b2
$(package)_patches=iostreams-106.patch signals2-noise.patch deprecated-two-arg-allocate.diff

ifneq ($(host_os),darwin)
$(package)_dependencies+=libcxx
endif

define $(package)_set_vars
$(package)_config_opts_release=variant=release
$(package)_config_opts_debug=variant=debug
$(package)_config_opts=--layout=system --user-config=user-config.jam
$(package)_config_opts+=threading=multi link=static -sNO_COMPRESSION=1
$(package)_config_opts_linux=target-os=linux threadapi=pthread runtime-link=shared
$(package)_config_opts_freebsd=cxxflags=-fPIC
$(package)_config_opts_darwin=target-os=darwin runtime-link=shared
$(package)_config_opts_mingw32=target-os=windows binary-format=pe threadapi=win32 runtime-link=static
$(package)_config_opts_x86_64=architecture=x86 address-model=64
$(package)_config_opts_i686=architecture=x86 address-model=32
$(package)_config_opts_aarch64=address-model=64
$(package)_config_opts_armv7a=address-model=32
ifneq (,$(findstring clang,$($(package)_cxx)))
$(package)_toolset_$(host_os)=clang
else
$(package)_toolset_$(host_os)=gcc
endif
$(package)_config_libraries=chrono,filesystem,program_options,system,thread,test
$(package)_cxxflags+=-std=c++17 -fvisibility=hidden
$(package)_cxxflags_linux=-fPIC
$(package)_cxxflags_freebsd=-fPIC

ifeq ($(host_os),freebsd)
  $(package)_ldflags+=-static-libstdc++ -lcxxrt
else
  $(package)_ldflags+=-static-libstdc++ -lc++abi
endif

endef

define $(package)_preprocess_cmds
  patch -p2 < $($(package)_patch_dir)/iostreams-106.patch && \
  patch -p2 < $($(package)_patch_dir)/signals2-noise.patch && \
  patch -p2 < $($(package)_patch_dir)/deprecated-two-arg-allocate.diff && \
  echo "using $($(package)_toolset_$(host_os)) : : $($(package)_cxx) : <cflags>\"$($(package)_cflags)\" <cxxflags>\"$($(package)_cxxflags)\" <compileflags>\"$($(package)_cppflags)\" <linkflags>\"$($(package)_ldflags)\" <archiver>\"$($(package)_ar)\" <striper>\"$(host_STRIP)\"  <ranlib>\"$(host_RANLIB)\" <rc>\"$(host_WINDRES)\" : ;" > user-config.jam
endef

define $(package)_config_cmds
  ./bootstrap.sh --without-icu --with-libraries=$($(package)_config_libraries) --with-toolset=$($(package)_toolset_$(host_os)) --with-bjam=b2
endef

define $(package)_build_cmds
  b2 -d2 -j2 -d1 --prefix=$($(package)_staging_prefix_dir) $($(package)_config_opts) toolset=$($(package)_toolset_$(host_os)) stage
endef

define $(package)_stage_cmds
  b2 -d0 -j4 --prefix=$($(package)_staging_prefix_dir) $($(package)_config_opts) toolset=$($(package)_toolset_$(host_os)) install
endef

# Boost uses the MSVC convention of libboost_foo.lib as the naming pattern when
# compiling for Windows, even though we use MinGW which follows the libfoo.a
# convention. This causes issues with lld, so we rename all .lib files to .a.
ifeq ($(host_os),mingw32)
define $(package)_postprocess_cmds
  for f in lib/*.lib; do mv -- "$$$$f" "$$$${f%.lib}.a"; done
endef
endif
