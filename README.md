# chimera [![Build Status](https://travis-ci.com/personalrobotics/chimera.svg?token=1MmAniN9fkMcwpRUTdFq&branch=master)](https://travis-ci.com/personalrobotics/chimera) [![codecov](https://codecov.io/gh/personalrobotics/chimera/branch/master/graph/badge.svg)](https://codecov.io/gh/personalrobotics/chimera) #

> ##### chi·me·ra #####
> ##### /kīˈmirə,kəˈmirə/ [![speaker][2]][1] #####
> _*informal*, noun_
>
> 1. a thing that is hoped or wished for but in fact is illusory or impossible to achieve.
> 2. a utility to generate Boost.Python bindings for C++ code.

Chimera is a tool for generating Boost.Python bindings from C/C++ header files.
It uses the Clang/LLVM toolchain, making it capable of automatically handling
fairly complex source files.

## Usage ##

```bash
$ ./chimera -c <yaml_config_file> -o <output_path> my_cpp_header1.h my_cpp_header2.h -- [compiler args]
```

## Installation ##

**Requirements**

- libclang 3.6 or above
- llvm 3.6 or above (+ tools)
- libedit
- yaml-cpp
- boost

**On Ubuntu from source**

```bash
$ sudo add-apt-repository 'deb http://llvm.org/apt/trusty/ llvm-toolchain-trusty-3.6 main'
$ wget -O - http://llvm.org/apt/llvm-snapshot.gpg.key | sudo apt-key add -
$ sudo apt-get update
$ sudo apt-get install llvm-3.6-dev llvm-3.6-tools libclang-3.6-dev libedit-dev libyaml-cpp-dev libboost-dev
$ git clone https://github.com/personalrobotics/chimera.git
$ cd chimera
$ mkdir build && cd build
$ cmake -DCMAKE_BUILD_TYPE=Release ..
$ make
$ sudo make install
```

**On macOS using Homebrew**
```bash
# install Homebrew if not installed
$ /usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
$ brew tap personalrobotics/tap
$ brew install chimera
```

**On macOS from source**

```bash
$ brew install boost llvm
$ brew install yaml-cpp --with-static-lib 
$ git clone https://github.com/personalrobotics/chimera.git
$ cd chimera
$ mkdir build && cd build
$ PKG_CONFIG_PATH=$(brew --prefix yaml-cpp)/lib/pkgconfig cmake -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_DIR=$(brew --prefix llvm)/lib/cmake/llvm ..
$ make
$ sudo make install
```

## Example ##
Let's try running chimera on itself!

```bash
$ cd [PATH TO CHIMERA]
$ rm -rf build && mkdir -p build && cd build
$ cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
$ make
$ chimera -p . -o chimera_py_binding.cpp ../src/chimera.cpp
```

## Configuration ##

```yaml
# The C++ namespaces that will be extracted by Chimera
namespaces:
  - dart::dynamics
  - dart::math

# Selected types that should have special handling.
# (Not implemented yet.)
types:
  'class BodyNode':
    convert_to: 'class BodyNodePtr'

# Selected function and class declarations that need custom parameters.
functions:
  'const Eigen::Vector4d & ::dart::dynamics::Shape::getRGBA() const':
    return_value_policy: ::boost::python::copy_const_reference
  'bool ::dart::dynamics::Skeleton::isImpulseApplied() const':
    source: 'test.cpp.in'
  'const Eigen::Vector3d & ::dart::dynamics::Shape::getBoundingBoxDim() const':
    content: '/* Instead of implementing this function, insert this comment! */'
  'Eigen::VectorXd & ::dart::optimizer::GradientDescentSolver::getEqConstraintWeights()': null
    # This declaration will be suppressed.

classes:
  '::dart::dynamics::Shape':
    name: Shape
    bases: []
    noncopyable: true
```

## Troubleshooting ##

### Is there a length limit for the keys in the configuration file of Chimera? ###

Yes. [`yaml-cpp` does not support more than 1024 characters for a single line
key](https://github.com/jbeder/yaml-cpp/blob/release-0.5.3/src/simplekey.cpp#L111).
If you want to use a longer key, then you should use [multi-line
key](http://stackoverflow.com/a/36295084).

## License ##
Chimera is released under the 3-clause BSD license. See [LICENSE](./LICENSE) for more
information.

## Authors ##
Chimera is developed by Michael Koval (**@mkoval**) and Pras Velagapudi (**@psigen**).

[1]: http://audio.oxforddictionaries.com/en/mp3/chimera_gb_1.mp3
[2]: https://upload.wikimedia.org/wikipedia/commons/7/74/Speaker_icon.svg
