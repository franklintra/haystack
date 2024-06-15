SERVER_USER=root
SERVER_IP=185.157.247.62
REMOTE_FOLDER="/project/$(whoami)/done"

# Open an interactive bash shell on the server
ssh -t $SERVER_USER@$SERVER_IP "bash --rcfile <(echo '. ~/.bashrc; cd $REMOTE_FOLDER && make clean && clear && source /pythonfk/bin/activate')"