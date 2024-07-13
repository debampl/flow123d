####
# Detect Git executable and retrieve some information from the repository.
# 
# Use variable: FLOW123D_SOURCE_DIR as working directory when calling git 
#               FLOW_MANUAL_VERSION 
#
# Module set (not cached) these variables:
# GIT_BRANCH - current git branch
# GIT_DESCRIBE - human-friendly description of current commit
#                (last tag + hash)
# GIT_URL - URL of remote repository 
#
# Consistent version info to be used on various places:
# For CPack versioning: 
# GIT_VERSION_MAJOR, GIT_VERSION_MINOR, GIT_VERSION_PATCH
# Version in flow123d executable, ref manual and in final package names
# GIT_VERSION_FULL
# 

include(FindGit)

if (GIT_FOUND) 
  
  #message(STATUS "PWD: ${FLOW123D_SOURCE_DIR}")
  # get GIT branch
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${FLOW123D_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

  # Get the describe string - tag + num. of commits since tag + short hash
  execute_process(
    COMMAND git describe
    WORKING_DIRECTORY ${FLOW123D_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_DESCRIBE
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  
  
  # Get remote branch we are tracking
  execute_process(
    COMMAND git branch --list -vv ${GIT_BRANCH}
    WORKING_DIRECTORY ${FLOW123D_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_FULL_BRANCH_INFO
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  STRING(REGEX REPLACE ".* \\[([^]]*)\\] .*" "\\1" GIT_REMOTE_BRANCH "${GIT_FULL_BRANCH_INFO}")
  STRING(REGEX REPLACE "([^\\w]*)/.*" "\\1" GIT_REMOTE "${GIT_REMOTE_BRANCH}")
  
  # detection of remote is unstable, use "origin" instead
  SET(GIT_REMOTE "origin")
  
  # Get Fetch URL of the remote
  execute_process(
    COMMAND git remote show -n ${GIT_REMOTE}
    WORKING_DIRECTORY ${FLOW123D_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_FULL_URL_INFO
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${FLOW123D_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_SHORT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  STRING(REGEX REPLACE "\"" "\\\\\"" GIT_FULL_URL_ESC_QUOTES "${GIT_FULL_URL_INFO}")
  STRING(REGEX MATCH "Fetch URL:[^\n]*" GIT_URL_TMP "${GIT_FULL_URL_ESC_QUOTES}")
  STRING(REGEX REPLACE "Fetch URL:([^\n]*)" "\\1" GIT_URL "${GIT_URL_TMP}")
  
  
  #message(STATUS "GIT_REMOTE: ${GIT_REMOTE}")
  #message(STATUS "GIT_BRANCH: ${GIT_BRANCH}")
  #message(STATUS "GIT_DESCRIBE: ${GIT_DESCRIBE}")
  #message(STATUS "GIT_REMOTE_BRANCH: ${GIT_REMOTE_BRANCH}")
  #message(STATUS "GIT_FULL_URL: ${GIT_FULL_URL_INFO}")
  #message(STATUS "GIT_FULL_URL_ESC_QUOTES: ${GIT_FULL_URL_ESC_QUOTES}")
  #message(STATUS "GIT_URL_TMP: ${GIT_URL_TMP}")
  #message(STATUS "GIT_URL: ${GIT_URL}")
  
  # Allow providing the version when calling the cmake or in cmake.config
  # Version is used in package builds.
  #message(STATUS "MANUAL VER: ${FLOW_MANUAL_VERSION}")
  if( NOT DEFINED FLOW_MANUAL_VERSION)
        # first try to read version from file
        FILE(READ ${FLOW123D_SOURCE_DIR}/version FLOW_MANUAL_VERSION)
  endif()

  #message(STATUS "MANUAL VER: ${FLOW_MANUAL_VERSION}")
  
  if(${FLOW_MANUAL_VERSION} MATCHES ".*([A-Za-z0-9_]*)\\.([A-Za-z0-9_]*)\\.([A-Za-z0-9_]*).*")
    # version stored in file is in correct format 
    # so we extract version components into list
    STRING(REGEX REPLACE "([A-Za-z0-9_]*)\\.([A-Za-z0-9_]*)\\.([A-Za-z0-9_]*)"
           "\\1;\\2;\\3" BRANCH_VERSION_LIST ${FLOW_MANUAL_VERSION})
    #message(STATUS "BRANCH VER LIST: ${BRANCH_VERSION_LIST}")
  
  elseif(${GIT_BRANCH} MATCHES "([A-Za-z0-9_]*)\\.([A-Za-z0-9_]*)\\.([A-Za-z0-9_]*)")
    # git branch is in correct format 
    # so we extract version components into list
    STRING(REGEX REPLACE "([A-Za-z0-9_]*)\\.([A-Za-z0-9_]*)\\.([A-Za-z0-9_]*)"
           "\\1;\\2;\\3" BRANCH_VERSION_LIST ${FLOW_MANUAL_VERSION})
  else()
    # no valid release version was found
    # version will be set to 0.0.x where x is branch and commit of current version
    SET(BRANCH_VERSION_LIST "0;0;${GIT_BRANCH}-${GIT_SHORT_HASH}")
  endif()
  
  string(STRIP "${BRANCH_VERSION_LIST}" BRANCH_VERSION_LIST)
  list(GET BRANCH_VERSION_LIST 0 GIT_VERSION_MAJOR)
  list(GET BRANCH_VERSION_LIST 1 GIT_VERSION_MINOR)
  list(GET BRANCH_VERSION_LIST 2 GIT_VERSION_PATCH)
      
  set(GIT_BRANCH ${GIT_BRANCH} CACHE INTERNAL "Current git branch.")
  set(GIT_DESCRIBE ${GIT_DESCRIBE} CACHE INTERNAL "Human readable description of the commit containing last annotated tag.")
  set(GIT_BRANCH_DESCRIBE ${GIT_BRANCH}-${GIT_SHORT_HASH} CACHE INTERNAL "Human readable description of the commit containing actual branch.") 
  set(GIT_SHORT_HASH ${GIT_SHORT_HASH} CACHE INTERNAL "Short hash of current commit.")
  set(GIT_URL ${GIT_URL} CACHE INTERNAL "URL of remote repository.")
  set(GIT_VERSION_MAJOR ${GIT_VERSION_MAJOR} CACHE INTERNAL "Major version component.")
  set(GIT_VERSION_MINOR ${GIT_VERSION_MINOR} CACHE INTERNAL "Minor version component.")
  set(GIT_VERSION_PATCH ${GIT_VERSION_PATCH} CACHE INTERNAL "Patch version component.")
  set(GIT_VERSION_FULL ${GIT_VERSION_MAJOR}.${GIT_VERSION_MINOR}.${GIT_VERSION_PATCH} CACHE INTERNAL "Full version name.")
  
  message(STATUS "======================================")
  message(STATUS "====== Git summary ===================")
  message(STATUS "======================================")
  message(STATUS "GIT_BRANCH:      ${GIT_BRANCH}")
  message(STATUS "GIT_DESCRIBE:    ${GIT_DESCRIBE}")
  message(STATUS "GIT_BRANCH_DESCRIBE:    ${GIT_BRANCH_DESCRIBE}")
  message(STATUS "GIT_URL:         ${GIT_URL}")
  message(STATUS "GIT_SHORT_HASH:  ${GIT_SHORT_HASH}")
  message(STATUS "GIT_VERSION_FULL: ${GIT_VERSION_FULL}")
endif()
