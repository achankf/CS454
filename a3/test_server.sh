TEST=PartialTestCode

BINDER_ADDRESS=$1
BINDER_PORT=$2
export BINDER_ADDRESS
export BINDER_PORT

$TEST/server

echo $?
