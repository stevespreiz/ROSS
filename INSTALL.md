# Installing ROSS the Spreizer Way

For the Eriks in our lives.

## Startup 

1. Clone (my version) of the repo into your local machine

```
cd ~
git clone -b master --single-branch git@github.com:stevespreiz/ROSS.git
cd ROSS
```

It big so need the master branch only.

2. Submodules (idk)

```
git submodule init
git submodule update
```

3. Link our model to ROSS (don't have to do this first time)

```
ln -s ~/path-to/our-model/in-the-github models/Project
```

4. Create build directory

```
cd ~/where-ever-you-want/
mkdir build-ROSS
cd build-ROSS
```

5. Load Modules and build

```
module load xl_r spectrum-mpi cuda
cmake3 ../ROSS -DCMAKE_CXX_COMPILER=mpicxx -DCMAKE_INSTALL_PREFIX=~/build-ROSS/ \
 -DCMAKE_C_COMPILER=gcc -DROSS_BUILD_MODELS=ON
make install -j4
```

6. Test that it does something

```
cd ~/build-ROSS/models/phold/
./phold_test
```

Should work I hope.
