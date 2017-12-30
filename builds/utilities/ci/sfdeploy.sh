#!/bin/bash
#
# Deploy LPub3D assets to sourceforge.net
#
#  Trevor SANDY <trevor.sandy@gmail.com>
#  Last Update: December 30, 2017
#  Copyright (c) 2017 by Trevor SANDY
#
#  Note: this script runs on AppVeyor (MinGW Bash)

echo "- Deploying to Sourceforge.net..." && echo

# set working directory
sfParent_dir=${PWD##*/}
if  [ "$sfParent_dir" = "ci" ]; then
  sfChkdir="$(realpath ../../../../)"
  [ -d "$chkdir" ] && sfWD=$sfChkdir || sfWD=$PWD
fi
echo "sfdeploy.sh working directory..[$sfWD]"

echo "- source set_bash_vars.sh..." && echo
source builds/utilities/ci/set_bash_vars.sh

# these must be exported
LP3D_SF_REMOTE_HOST=216.34.181.57 # frs.sourceforge.net

# add ssh key to ssh-agent
mv -f "builds/utilities/ci/sfdeploy_appveyor_rsa" "/tmp/sfdeploy_rsa"
eval "$(ssh-agent -s)"
chmod 600 /tmp/sfdeploy_rsa
ssh-add /tmp/sfdeploy_rsa

# add host public key to known_hosts - prevent interactive prompt
[ -z `ssh-keygen -F $LP3D_SF_REMOTE_HOST` ] && ssh-keyscan -H $IP >> ~/.ssh/known_hosts

# upload update and download assets
for OPTION in UDPATE DOWNLOAD; do
  case $OPTION in
  UDPATE)
    # Verify release files in the Update directory
    if [ -n "$(find "$LP3D_BUILD_UPDATE_TARGET" -maxdepth 0 -type d -empty 2>/dev/null)" ]; then
      echo "$LP3D_BUILD_UPDATE_TARGET is empty. Sourceforge.net update assets deploy aborted.";
    else
      echo "Download Release Assets:" && find $LP3D_BUILD_UPDATE_TARGET -type f
	    rsync -r --quiet $LP3D_BUILD_UPDATE_TARGET/* $LP3D_SF_UDPATE_CONNECT/
	  fi
    ;;
  DOWNLOAD)
    # Verify release files in the Download directory
    if [ -n "$(find "$LP3D_BUILD_DOWNLOAD_TARGET" -maxdepth 0 -type d -empty 2>/dev/null)" ]; then
      echo "$LP3D_BUILD_DOWNLOAD_TARGET is empty. Sourceforge.net download assets deploy aborted.";
    else
      echo "Download Release Assets:" && find $LP3D_BUILD_DOWNLOAD_TARGET -type f;
	    rsync -r --quiet $LP3D_BUILD_DOWNLOAD_TARGET/* $LP3D_SF_DOWNLOAD_CONNECT/$LP3D_VERSION/
	  fi
    ;;
  esac
done