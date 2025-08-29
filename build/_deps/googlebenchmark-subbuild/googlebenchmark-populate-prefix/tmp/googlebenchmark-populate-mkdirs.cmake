# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/bararide/code/sem6/Cpp/owl/build/_deps/googlebenchmark-src"
  "/home/bararide/code/sem6/Cpp/owl/build/_deps/googlebenchmark-build"
  "/home/bararide/code/sem6/Cpp/owl/build/_deps/googlebenchmark-subbuild/googlebenchmark-populate-prefix"
  "/home/bararide/code/sem6/Cpp/owl/build/_deps/googlebenchmark-subbuild/googlebenchmark-populate-prefix/tmp"
  "/home/bararide/code/sem6/Cpp/owl/build/_deps/googlebenchmark-subbuild/googlebenchmark-populate-prefix/src/googlebenchmark-populate-stamp"
  "/home/bararide/code/sem6/Cpp/owl/build/_deps/googlebenchmark-subbuild/googlebenchmark-populate-prefix/src"
  "/home/bararide/code/sem6/Cpp/owl/build/_deps/googlebenchmark-subbuild/googlebenchmark-populate-prefix/src/googlebenchmark-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/bararide/code/sem6/Cpp/owl/build/_deps/googlebenchmark-subbuild/googlebenchmark-populate-prefix/src/googlebenchmark-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/bararide/code/sem6/Cpp/owl/build/_deps/googlebenchmark-subbuild/googlebenchmark-populate-prefix/src/googlebenchmark-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
