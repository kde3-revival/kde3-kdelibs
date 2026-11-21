#!/bin/bash
# The Secret Service backend of kwalletd against a private keyring.
#
#   run-tests.sh <kdelibs build dir> [xdrv]
#
# Everything is private: a temporary HOME/KDEHOME/XDG dirs, a private
# D-Bus session (no service activation: only what this script starts), a
# private gnome-keyring-daemon unlocked with a test password, a headless
# mutter with Xwayland on a display from :290 up for kded's dialogs.  The
# user's keyring, wallets and session bus are never touched.
#
# The kded part needs the installed KDE 3 (/opt/kde3: dcopserver, kded,
# kbuildsycoca) and loads kded_kwalletd.so from the build dir; xdrv
# (XSendEvent key presses to a process's window) answers the dialogs of the
# migration, without it that part is skipped.
set -u
B=$(cd "${1:?build dir}" && pwd)
XDRV=${2:-}
SRC=$(cd "$(dirname "$0")" && pwd)

if [ -z "${KWSS_INNER:-}" ]; then
    mkdir -p /tmp/claude-1000 2>/dev/null
    T=$(mktemp -d "${TMPDIR:-/tmp}/kwss-test.XXXXXX")
    mkdir -p "$T"/{home,kde,tmp,vartmp,run,data,config,cache,services,prefix}
    chmod 700 "$T/run"
    # a session bus without the system's service files: nothing is
    # activated, the real gnome-keyring is never started here
    sed "s|<standard_session_servicedirs */>|<servicedir>$T/services</servicedir>|" \
        /usr/share/dbus-1/session.conf > "$T/session.conf"
    env -i PATH="$PATH" HOME="$T/home" KDEHOME="$T/kde" KDETMP="$T/tmp" KDEVARTMP="$T/vartmp" \
        XDG_RUNTIME_DIR="$T/run" XDG_DATA_HOME="$T/data" XDG_CONFIG_HOME="$T/config" \
        XDG_CACHE_HOME="$T/cache" KWSS_INNER="$T" LANG=C.UTF-8 \
        dbus-run-session --config-file="$T/session.conf" -- "$0" "$B" "$XDRV"
    rc=$?
    [ -n "${KEEP:-}" ] && echo "kept $T" || rm -rf "$T"
    exit $rc
fi

T=$KWSS_INNER
FAILED=0
PW=test-login-password
SST=$B/kio/misc/kwalletd/tests/kwalletd_sstest
KWT=$B/kio/misc/kwalletd/tests/kwalletd_kwtest
MKKWL=$B/kio/misc/kwalletd/tests/kwalletd_mkkwl
PIDS=""
cleanup() { for p in $PIDS; do kill $p 2>/dev/null; done; wait 2>/dev/null; }
trap cleanup EXIT

step() { echo "== $*"; }
run() { "$@" | sed 's/^/   /'; local rc=${PIPESTATUS[0]}; [ $rc = 0 ] || { echo "   -> exit $rc"; FAILED=1; }; }
expect() { if eval "$2"; then echo "   PASS $1"; else echo "   FAIL $1"; FAILED=1; fi; }
has_service() { dbus-send --session --print-reply --dest=org.freedesktop.DBus / org.freedesktop.DBus.NameHasOwner string:org.freedesktop.secrets 2>/dev/null | grep -q true; }

KEYRING=""
start_keyring() {
    printf %s "$PW" | gnome-keyring-daemon --foreground --unlock --components=secrets > "$T/keyring.log" 2>&1 &
    KEYRING=$!; PIDS="$PIDS $KEYRING"
    for i in $(seq 50); do has_service && return 0; sleep 0.1; done
    echo "gnome-keyring-daemon did not start"; cat "$T/keyring.log"; exit 1
}
stop_keyring() { kill $KEYRING; wait $KEYRING 2>/dev/null; for i in $(seq 50); do has_service || return 0; sleep 0.1; done; }

step "private keyring"
start_keyring
expect "the session bus is private" '[ "$(dbus-send --session --print-reply --dest=org.freedesktop.DBus / org.freedesktop.DBus.ListActivatableNames | grep -c secrets)" = 0 ]'
# an item of another program, with the same attribute names
printf %s foreign-secret | secret-tool store --label=foreign wallet kdewallet folder Passwords key foreign type password

step "store: every operation"
run "$SST" store
step "store: read back by a new store"
run "$SST" check
expect "a password is text/plain UTF-8 in the keyring" \
    '[ "$(secret-tool lookup xdg:schema org.kde.kwallet3 wallet kdewallet folder Passwords key plain type password)" = "sécret ü 🔑" ]'
expect "the foreign item is untouched" '[ "$(secret-tool lookup wallet kdewallet folder Passwords key foreign)" = foreign-secret ]'
expect "the removed folder left no item" '[ -z "$(secret-tool search --all xdg:schema org.kde.kwallet3 folder Doomed 2>&1)" ]'
expect "no plaintext secret on disk" '! grep -rqa "sécret ü" "$T/data" "$T/home" "$T/kde" "$T/config"'

step "the service disappears under an open store"
coproc VAN { "$SST" vanish; }
read -r line <&"${VAN[0]}"
stop_keyring
echo go >&"${VAN[1]}"
while read -r line <&"${VAN[0]}"; do echo "   $line"; case $line in FAIL*) FAILED=1;; esac; done
step "the service is gone"
run "$SST" down

step "locked collection, the unlock prompt cannot be shown"
start_keyring
secret-tool lock --collection=/org/freedesktop/secrets/collection/login
run "$SST" locked
stop_keyring
# remove the store test's wallet: the kded part starts empty
start_keyring
secret-tool clear xdg:schema org.kde.kwallet3 wallet kdewallet 2>/dev/null
while secret-tool clear xdg:schema org.kde.kwallet3 2>/dev/null && secret-tool search xdg:schema org.kde.kwallet3 2>&1 | grep -q .; do :; done

# ---- kded + DCOP + KWallet::Wallet
for D in $(seq 290 330); do [ -e /tmp/.X$D-lock ] || [ -e /tmp/.X11-unix/X$D ] || break; done
step "headless mutter, Xwayland :$D"
mutter --headless --wayland --no-x11 --virtual-monitor 1280x800 --wayland-display wl-kwss > "$T/mutter.log" 2>&1 &
PIDS="$PIDS $!"
for i in $(seq 50); do [ -S "$T/run/wl-kwss" ] && break; sleep 0.1; done
WAYLAND_DISPLAY=wl-kwss Xwayland :$D -noreset > "$T/xwayland.log" 2>&1 &
PIDS="$PIDS $!"
for i in $(seq 50); do [ -e /tmp/.X11-unix/X$D ] && break; sleep 0.1; done
export DISPLAY=:$D
case $DISPLAY in :0|:1) echo "refusing $DISPLAY"; exit 1;; esac

mkdir -p "$T/prefix/lib/kde3" "$T/prefix/share/services/kded" "$T/kde/share/config"
cp "$B/kio/misc/kwalletd/kded_kwalletd.so" "$T/prefix/lib/kde3/"
cp "$SRC/../kwalletd.desktop" "$T/prefix/share/services/kded/"
export KDEDIRS="$T/prefix:/opt/kde3"
printf '[Wallet]\nLaunch Manager=false\n' > "$T/kde/share/config/kwalletrc"
/opt/kde3/bin/dcopserver --nosid --nofork > "$T/dcop.log" 2>&1 &
PIDS="$PIDS $!"
sleep 1
/opt/kde3/bin/kbuildsycoca --noincremental > "$T/sycoca.log" 2>&1
/opt/kde3/bin/kded --nofork > "$T/kded.log" 2>&1 &
KDED=$!; PIDS="$PIDS $KDED"
for i in $(seq 50); do dcop kded >/dev/null 2>&1 && break; sleep 0.2; done
expect "kded uses the module of the build" 'grep -q "$T/prefix/lib/kde3/kded_kwalletd.so" /proc/$KDED/maps || { dcop kded kwalletd isEnabled >/dev/null; sleep 0.5; grep -q "$T/prefix/lib/kde3/kded_kwalletd.so" /proc/$KDED/maps; }'
expect "kwalletd uses the Secret Service" '[ "$(dcop kded kwalletd usesSecretService)" = true ]'

step "KWallet::Wallet: every call"
run "$KWT" api
step "KWallet::Wallet: read back by another process"
run "$KWT" reread
step "KMail: write, then read in a second run"
run "$KWT" kmail
run "$KWT" kmail
step "KHTML forms: asynchronous open, map"
run "$KWT" khtml
run "$KWT" khtml
expect "no wallet file was written" '[ -z "$(ls "$T/kde/share/apps/kwallet" 2>/dev/null)" ]'
expect "no secret in the config" '! grep -rqa -e "pässwörd" -e imap-secret -e p4ss "$T/kde" "$T/config" "$T/home"'

if [ -n "$XDRV" ]; then
    step "migration of a classic wallet (password oldpw)"
    dcop kded kwalletd closeAllWallets
    mkdir -p "$T/kde/share/apps/kwallet"
    "$MKKWL" "$T/kde/share/apps/kwallet/kdewallet.kwl" oldpw
    "$KWT" migrated > "$T/migrated.out" 2>&1 &
    M=$!
    sleep 3; "$XDRV" $KDED key:Return              # "Import"
    sleep 2; "$XDRV" $KDED key:o key:l key:d key:p key:w key:Return
    sleep 2; "$XDRV" $KDED key:Return              # the summary
    wait $M; mrc=$?
    sed 's/^/   /' "$T/migrated.out"
    [ $mrc = 0 ] || FAILED=1
    expect "the classic file is kept as .kwl.migrated" '[ -f "$T/kde/share/apps/kwallet/kdewallet.kwl.migrated" ] && [ ! -e "$T/kde/share/apps/kwallet/kdewallet.kwl" ]'
    expect "the import is recorded" 'grep -q "^kdewallet=done" "$T/kde/share/config/kwalletrc"'
    expect "no second prompt: the wallet opens" '"$KWT" reread >/dev/null'

    step "Backend=File: the classic wallet (password oldpw)"
    dcop kded kwalletd closeAllWallets
    printf '[Wallet]\nBackend=File\nLaunch Manager=false\nFirst Use=false\n' > "$T/kde/share/config/kwalletrc"
    dcop kded kwalletd reconfigure
    expect "kwalletd uses the files" '[ "$(dcop kded kwalletd usesSecretService)" = false ]'
    cp "$T/kde/share/apps/kwallet/kdewallet.kwl.migrated" "$T/kde/share/apps/kwallet/kdewallet.kwl"
    "$KWT" classic > "$T/classic.out" 2>&1 &
    M=$!
    sleep 3; "$XDRV" $KDED key:o key:l key:d key:p key:w key:Return
    wait $M; mrc=$?
    sed 's/^/   /' "$T/classic.out"
    [ $mrc = 0 ] || FAILED=1
    expect "the entry went into the file, not the keyring" '! secret-tool search xdg:schema org.kde.kwallet3 key added 2>&1 | grep -q .'
    dcop kded kwalletd closeAllWallets
    printf '[Wallet]\nLaunch Manager=false\nFirst Use=false\n' > "$T/kde/share/config/kwalletrc"
    dcop kded kwalletd reconfigure
    expect "back on the Secret Service" '[ "$(dcop kded kwalletd usesSecretService)" = true ]'
fi

step "the service is gone: opening fails without a password dialog"
dcop kded kwalletd closeAllWallets
stop_keyring
run "$KWT" fails
expect "no dialog of kded" '! xwininfo -root -tree 2>/dev/null | grep -q "KDE Wallet Service"'

echo
[ $FAILED = 0 ] && echo "ALL PASSED" || echo "SOME FAILED (logs: KEEP=1)"
exit $FAILED
