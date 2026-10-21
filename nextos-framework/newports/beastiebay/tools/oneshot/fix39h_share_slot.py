import io

p = 'port/src/jni.c'
d = io.open(p, encoding='utf-8', newline='').read()

# 1) Drop the dedicated xthrow slot.  The throw stub now publishes through the
#    SAME slot as the method stubs -- one variable, written by whichever
#    hook installed last, read by whichever stub is running.  That is wrong in
#    general and right here: fp2_install_throw_hook() runs AFTER all five
#    method hooks, and the throw stub is the only consumer between the moment
#    it publishes and the next method-hook install (which never happens again,
#    fp2_install_changestate_hook() is idempotent).
old = u'FP2_TRAMP(xthrow);'
assert d.count(old) == 1, d.count(old)
d = d.replace(old + u'\n', u'')

# 2) Point the throw stub's tail at the `init` slot, which the throw hook
#    overwrites at install time.
old2 = u'FP2_W_TAIL(xthrow)'
assert d.count(old2) == 1, d.count(old2)
d = d.replace(old2, u'FP2_W_TAIL(init)')

# 3) Publish through that slot.
old3 = u'    fp2_watch_tramp_xthrow = fp2_throw_hook.trampoline;'
assert d.count(old3) == 1, d.count(old3)
d = d.replace(
    old3,
    u'    /* Published into the `init` slot, on purpose -- see the note above\n'
    u'     * the throw stub.  fp2_install_throw_hook() runs after every method\n'
    u'     * hook and fp2_install_changestate_hook() is idempotent, so nothing\n'
    u'     * overwrites it afterwards. */\n'
    u'    fp2_watch_tramp_init = fp2_throw_hook.trampoline;')

io.open(p, 'w', encoding='utf-8', newline='').write(d)
print('ok')
