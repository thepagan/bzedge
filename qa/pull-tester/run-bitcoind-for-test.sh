#!/bin/bash
# Copyright (c) 2013-2014 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .
#
ZCASH_LOAD_TIMEOUT=500
DATADIR="/home/miodrag/devel/dev-bze/alphateam/bzedge/.zcash"
rm -rf "$DATADIR"
mkdir -p "$DATADIR"/regtest
touch "$DATADIR/zcash.conf"
touch "$DATADIR/regtest/debug.log"
tail -q -n 1 -F "$DATADIR/regtest/debug.log" | grep -m 1 -q "Done loading" &
WAITER=$!
PORT=`expr 10000 + $$ % 55536`
"/home/miodrag/devel/dev-bze/alphateam/bzedge/src/zcashd" -connect=0.0.0.0 -datadir="$DATADIR" -rpcuser=user -rpcpassword=pass -listen -keypool=3 -debug -debug=net -logtimestamps -checkmempool=0 -relaypriority=0 -port=$PORT -whitelist=127.0.0.1 -regtest -rpcport=`expr $PORT + 1` &
BITCOIND=$!

#Install a watchdog.
(sleep "$ZCASH_LOAD_TIMEOUT" && kill -0 $WAITER 2>/dev/null && kill -9 $BITCOIND $$)&
wait $WAITER

if [ -n "$TIMEOUT" ]; then
  timeout "$TIMEOUT"s "$@" $PORT
  RETURN=$?
else
  "$@" $PORT
  RETURN=$?
fi

(sleep "$ZCASH_LOAD_TIMEOUT" && kill -0 $BITCOIND 2>/dev/null && kill -9 $BITCOIND $$)&
kill $BITCOIND && wait $BITCOIND

# timeout returns 124 on timeout, otherwise the return value of the child

# If $RETURN is not 0, the test failed. Dump the tail of the debug log.
if [ $RETURN -ne 0 ]; then tail -n 200 $DATADIR/regtest/debug.log; fi

exit $RETURN
