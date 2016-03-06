BINDER_ADDRESS=$2
BINDER_PORT=$3
export BINDER_ADDRESS
export BINDER_PORT

#valgrind --leak-check=full --track-origins=yes --show-reachable=yes $1
$1

echo $?
