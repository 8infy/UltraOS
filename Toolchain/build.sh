MYDIR="$(dirname "$(realpath "$0")")"

on_error()
{
    echo "Cross-compiler build failed!"
    exit 1
}

if [ -e "$MYDIR/CrossCompiler/tools/bin/i686-elf-g++" ]
then
  echo "Cross-compiler is already built!"
  exit 0
else
  echo "Building the cross-compiler..."
fi

sudo apt update

# should also include makefile/gcc?
declare -a dependencies=(
            "bison"
            "flex"
            "libgmp-dev"
            "libmpc-dev"
            "libmpfr-dev"
            "texinfo"
            "libisl-dev"
            "build-essential"
        )

for dependency in "${dependencies[@]}"
do
   echo -n $dependency
   is_dependency_installed=$(dpkg-query -l | grep $dependency)
   
   if [ -z "$is_dependency_installed" ]
    then
      echo " - not installed"
      sudo apt install -y $dependency || on_error
    else
      echo " - installed"
    fi
done

gcc_version="gcc-9.3.0"
binutils_version="binutils-2.34"
gcc_url="ftp://ftp.gnu.org/gnu/gcc/$gcc_version/$gcc_version.tar.gz"
binutils_url="https://ftp.gnu.org/gnu/binutils/$binutils_version.tar.gz"

if [ ! -e "$MYDIR/CrossCompiler/gcc/configure" ]
then
  echo "Downloading GCC source files..."
  mkdir -p "$MYDIR/CrossCompiler" || on_error
  mkdir -p "$MYDIR/CrossCompiler/gcc" || on_error
  wget -O "$MYDIR/CrossCompiler/gcc.tar.gz" $gcc_url || on_error
  echo "Unpacking GCC source files..."
  tar -xf "$MYDIR/CrossCompiler/gcc.tar.gz" \
      -C  "$MYDIR/CrossCompiler/gcc" --strip-components 1  || on_error
  rm "$MYDIR/CrossCompiler/gcc.tar.gz"
else
  echo "GCC is already downloaded!"
fi

if [ ! -e "$MYDIR/CrossCompiler/binutils/configure" ]
then
  echo "Downloading binutils source files..."
  mkdir -p  "$MYDIR/CrossCompiler" || on_error
  mkdir -p  "$MYDIR/CrossCompiler/binutils" || on_error
  wget -O "$MYDIR/CrossCompiler/binutils.tar.gz" $binutils_url || on_error
  echo "Unpacking binutils source files..."
  tar -xf "$MYDIR/CrossCompiler/binutils.tar.gz" \
      -C "$MYDIR/CrossCompiler/binutils" --strip-components 1 || on_error
  rm "$MYDIR/CrossCompiler/binutils.tar.gz"
else
  echo "binutils is already downloaded!"
fi

export PREFIX="$MYDIR/CrossCompiler/tools"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"

echo "Building binutils..."
mkdir -p "$MYDIR/CrossCompiler/binutils_build" || on_error

pushd "$MYDIR/CrossCompiler/binutils_build"
../binutils/configure --target=$TARGET \
                      --prefix="$PREFIX" \
                      --with-sysroot \
                      --disable-nls \
                      --disable-werror || on_error
make || on_error
make install || on_error
popd

echo "Building GCC..."
mkdir -p "$MYDIR/CrossCompiler/gcc_build" || on_error
pushd "$MYDIR/CrossCompiler/gcc_build"
../gcc/configure --target=$TARGET \
                 --prefix="$PREFIX" \
                 --disable-nls \
                 --enable-languages=c,c++ \
                 --without-headers || on_error

make all-gcc || on_error
make all-target-libgcc || on_error
make install-gcc || on_error
make install-target-libgcc || on_error
popd

# consider deleting the intermediate folders here
echo "Cross-compiler built successfully!"