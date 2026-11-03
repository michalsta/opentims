#!/bin/bash
# Check an opentimsr source tarball the way CRAN's r-devel-linux-x86_64-fedora-clang
# machine does, following its published setup:
#   https://www.stats.ox.ac.uk/pub/bdr/Rconfig/r-devel-linux-x86_64-fedora-clang
# Fedora 44, LLVM's own binary release with libc++, R-devel built from source
# with it, and every dependency built from source with it too. System C++
# libraries (ICU) stay Fedora's gcc/libstdc++ builds, as on CRAN, so libc++
# and libstdc++ end up in one process -- what broke opentimsr 1.2.0 there.
#
# Differences from CRAN's machine: no X11, no recommended packages, no PDF
# manual (--no-manual, as in r_check.yml).
#
# Run as root in a fresh fedora:44 container; it installs into the system.
# Usage: fedora_clang_check.sh <opentimsr_X.tar.gz> <check dir>
set -euo pipefail
tarball="$(realpath "$1")"
check_dir="$2"

LLVM_VERSION="${LLVM_VERSION:-23.1.4}"
PANDOC_VERSION="${PANDOC_VERSION:-3.12.1}"
llvm=/usr/local/llvm
prefix=/opt/R-devel
jobs="$(nproc)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

echo "::group::System packages"
dnf install -y -q --setopt=install_weak_deps=False \
    gcc libquadmath make perl which findutils diffutils file tar xz zstd \
    readline-devel libcurl-devel bzip2-devel xz-devel pcre2-devel zlib-devel \
    libicu-devel libzstd-devel libuv-devel tidy devscripts-checkbashisms \
    glibc-langpack-en tzdata
echo "::endgroup::"

# Only the compilers, headers and runtimes: the whole release unpacks to 12 GB.
# It is compressed with a 1 GB window, which zstd decodes only with --long.
echo "::group::LLVM $LLVM_VERSION"
top="LLVM-$LLVM_VERSION-Linux-X64"
major="${LLVM_VERSION%%.*}"
mkdir -p "$llvm"
curl -fsSL "https://github.com/llvm/llvm-project/releases/download/llvmorg-$LLVM_VERSION/$top.tar.zst" |
    tar -I 'zstd -d --long=31' -x -C "$llvm" --strip-components=1 \
        "$top/bin/clang" "$top/bin/clang++" "$top/bin/clang-$major" \
        "$top/bin/flang" "$top/bin/flang-$major" \
        "$top/include/c++" "$top/include/x86_64-unknown-linux-gnu" "$top/include/flang" \
        "$top/lib/clang" "$top/lib/x86_64-unknown-linux-gnu" "$top/lib/libFortranDecimal.a"
printf '%s\n' "$llvm/lib/x86_64-unknown-linux-gnu" "$llvm/lib" > /etc/ld.so.conf.d/llvm.conf
ldconfig
"$llvm/bin/clang" --version
echo "::endgroup::"

# Fedora's pandoc is too old for CRAN; it uses pandoc's own binary.
echo "::group::pandoc $PANDOC_VERSION"
curl -fsSL "https://github.com/jgm/pandoc/releases/download/$PANDOC_VERSION/pandoc-$PANDOC_VERSION-linux-amd64.tar.gz" |
    tar -xz -C /usr/local --strip-components=1
pandoc --version | head -n 1
echo "::endgroup::"

# CRAN's config.site, with our LLVM paths. CRAN's LLVM defaults to libc++;
# a release binary needs -stdlib=libc++, as the setup page says.
echo "::group::R-devel"
cat > "$work/config.site" <<EOF
CC=$llvm/bin/clang
CXX="$llvm/bin/clang++ -stdlib=libc++"
OBJC=$llvm/bin/clang
FC=$llvm/bin/flang
CFLAGS="-O3 -Wall -pedantic -Wp,-D_FORTIFY_SOURCE=3"
C17FLAGS="-O3 -Wall -pedantic -Wno-strict-prototypes -Wp,-D_FORTIFY_SOURCE=3"
FFLAGS="-O2 -pedantic"
CXXFLAGS="-O3 -Wall -pedantic -frtti -Wp,-D_FORTIFY_SOURCE=3 -Wno-missing-template-arg-list-after-template-kw"
CPPFLAGS="-isystem $llvm/include"
LDFLAGS="-L$llvm/lib -L$llvm/lib/x86_64-unknown-linux-gnu"
EOF
curl -fsSL https://cran.r-project.org/src/base-prerelease/R-devel.tar.xz | tar -xJ -C "$work"
mkdir "$work/build"
(
    cd "$work/build"
    CONFIG_SITE="$work/config.site" "$work/R-devel/configure" --prefix="$prefix" \
        --without-lapack --with-x=no --without-recommended-packages
    make -j"$jobs"
    make install
) > "$work/R-build.log" 2>&1 || { tail -n 100 "$work/R-build.log"; exit 1; }
export PATH="$prefix/bin:$PATH"
R --version | head -n 1
echo "::endgroup::"

# CRAN's check environment for this flavour.
export LANG=en_GB.UTF-8 LC_COLLATE=C TZ=Europe/London
export OMP_THREAD_LIMIT=2 MAKEFLAGS="-j$jobs"
export _R_CHECK_CRAN_INCOMING_REMOTE_=false
export _R_CHECK_INSTALL_DEPENDS_=true
export _R_CHECK_SUGGESTS_ONLY_=true
export _R_CHECK_NO_RECOMMENDED_=true
export _R_CHECK_DOC_SIZES2_=true
export _R_CHECK_DEPRECATED_DEFUNCT_=true
export _R_CHECK_SCREEN_DEVICE_=warn
export _R_CHECK_REPLACING_IMPORTS_=true
export _R_CHECK_TOPLEVEL_FILES_=true
export _R_CHECK_DOT_FIRSTLIB_=true
export _R_CHECK_RD_LINE_WIDTHS_=true
export _R_CHECK_S3_METHODS_NOT_REGISTERED_=true
export _R_CHECK_OVERWRITE_REGISTERED_S3_METHODS_=true
export _R_CHECK_CODE_USAGE_WITH_ONLY_BASE_ATTACHED_=TRUE
export _R_CHECK_NATIVE_ROUTINE_REGISTRATION_=true
export _R_CHECK_FF_CALLS_=registration
export _R_CHECK_PRAGMAS_=true
export _R_CHECK_COMPILATION_FLAGS_=true
export _R_CHECK_R_DEPENDS_=true
export _R_CHECK_PACKAGES_USED_IN_TESTS_USE_SUBDIRS_=true
export _R_CHECK_SHLIB_OPENMP_FLAGS_=true
export _R_CHECK_BROWSER_NONINTERACTIVE_=true
export _R_CHECK_CODE_ASSIGN_TO_GLOBALENV_=true
export _R_CHECK_CODE_DATA_INTO_GLOBALENV_=true
export _R_CHECK_PKG_SIZES_=true
export _R_CHECK_LIMIT_CORES_=true
export _R_S3_METHOD_LOOKUP_BASEENV_AFTER_GLOBALENV_=true
export _R_CHECK_AUTOCONF_=true
export _R_CHECK_THINGS_IN_CHECK_DIR_=true
export _R_CHECK_THINGS_IN_TEMP_DIR_=true
export _R_CHECK_BASHISMS_=true
export _R_CHECK_ORPHANED_=true
export _R_CHECK_DEPENDS_ONLY_DATA_=true
export _R_CHECK_XREFS_PKGS_ARE_DECLARED_=true
export _R_CHECK_BOGUS_RETURN_=true
export _R_CHECK_MATRIX_DATA_=TRUE
export _R_CHECK_XREFS_USE_ALIASES_FROM_CRAN_=TRUE
export _R_CHECK_MBCS_CONVERSION_FAILURE_=true
export _R_CHECK_RD_VALIDATE_RD2HTML_=true
export _R_CHECK_RD_MATH_RENDERING_=true
export _R_CHECK_COMPILATION_FLAGS_KNOWN_='-Werror=implicit-function-declaration -Wno-error=enum-constexpr-conversion -Wp,-D_FORTIFY_SOURCE=3 -Wno-missing-template-arg-list-after-template-kw'

# From source, so Rcpp and RSQLite are built against libc++ as on CRAN.
echo "::group::Dependencies"
tar -xzf "$tarball" -C "$work" opentimsr/DESCRIPTION
Rscript - "$work/opentimsr/DESCRIPTION" <<'EOF'
d <- read.dcf(commandArgs(TRUE)[1], c("Imports", "LinkingTo", "Suggests"))
pkgs <- trimws(sub("\\(.*", "", unlist(strsplit(d[!is.na(d)], ","))))
pkgs <- setdiff(unique(pkgs), rownames(installed.packages(priority = "base")))
install.packages(pkgs, repos = "https://cloud.r-project.org", Ncpus = parallel::detectCores())
missing <- pkgs[!vapply(pkgs, requireNamespace, logical(1), quietly = TRUE)]
if (length(missing)) stop("not installed: ", paste(missing, collapse = ", "))
EOF
echo "::endgroup::"

mkdir -p "$check_dir"
cd "$check_dir"
R CMD check --as-cran --no-manual "$tarball" || true

echo "::group::C++ runtimes loaded with opentimsr"
R_LIBS=opentimsr.Rcheck Rscript -e 'library(opentimsr)' \
    -e 'm <- readLines("/proc/self/maps")' \
    -e 'cat(unique(sub(".* ", "", grep("lib(c|stdc)\\+\\+\\.so", m, value = TRUE))), sep = "\n")' || true
echo "::endgroup::"

# Like rcmdcheck's error_on = "warning" in r_check.yml: NOTEs pass.
status="$(grep '^Status:' opentimsr.Rcheck/00check.log || echo 'Status: no result')"
echo "$status"
if echo "$status" | grep -Eq 'ERROR|WARNING|no result'; then
    exit 1
fi
