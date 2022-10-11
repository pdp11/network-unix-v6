/*
 * Rudimentary error routine.
 */
_serror(msg)
    char * msg;
{
    register char * intro;
    extern int slength();

    intro = "From slib: ";
    write(2, intro, slength(intro));

    write(2, msg, slength(msg));

    intro = "(aborting)\n";
    write(2, intro, slength(intro));
    abort();
}
