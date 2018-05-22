main()		/* run with set-ID to a suitable ID so can get mounted info */
{	execl("/etc/mount", "mounted", 0);
}
