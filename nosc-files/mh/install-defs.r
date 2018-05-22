if -r install-defs rm install-defs
load install-defs subs.a strings.a wait -O /m/mh/libg.a /m/mh/libh.a
if -r a.out mv a.out install-defs
:					fini

