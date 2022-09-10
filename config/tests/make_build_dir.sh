#!/bin/bash
# Usage:
#  
#     make_packages.sh <environment> <target_image>]
# 

set -e
set -x

environment=$1
if [ -z $environment ]
    then exit 1
fi	    


#######################################
# Paths on host
# TODO: move both version and image_tag to the same location in package
flow_repo_host="$( cd "$( dirname "${BASH_SOURCE[0]}" )"/../.. && pwd )"

################################
# paths inside docker container
flow_repo_location=/opt/flow123d/flow123d


#####################
# Docker calls
build_container=flow123d_ci_debug_build




###############################
# Conatiners and images names and tags
imagesversion=`cat ${flow_repo_host}/config/docker/image_tag`
#release_version=`cat ${flow_repo_host}/version`      

build_type=dbg
build_image="flow-dev-${environment}-${build_type}:${imagesversion}"


git_hash=`git rev-parse --short=6 HEAD`
git_branch=`git rev-parse --abbrev-ref HEAD`
build_dir_host=build-${git_branch}


echo "Variables summary"
echo "build_container: '${build_container}'"
echo "environment: '${environment}'"
echo "imagesversion: '${imagesversion}'"
echo "build image: '${build_image}'"
#echo "image_tag: '${image_tag}'"
#echo "target_image: '${target_image}'"



######################################################################################################### setup container

# docker rm -f  || echo "container not running"
bin/fterm update
#docker rm -f ${build_container}    
# TODO:
# Unable to create named container with fterm (--detach was intended to that purpose but never truly worked).
# So as far as whole work is done on the bind volume we can just create new container for every command just using 'bin/fterm run'
# rather prevent --priviledged as then git complains as the git user differs from the priviledged user
# more over resulting files have wrong permissions

# Build flow123d binary (and flow123d libraries)
if [ -L build_tree ]
then rm build_tree
fi
cp config/config-jenkins-docker-${build_type}.cmake config.cmake
bin/fterm dbg_${environment}  --no-term exec make -j4 all

# Basic test of working binary.
bin/fterm --no-term run --version

# Tarball the build dir for reuse.
cp config/config-jenkins-docker-${build_type}.cmake build_tree/_config.cmake
ls 
tar -cvf build_dir.tar ${build_dir_host}/* 

