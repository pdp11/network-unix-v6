
/*	measure.h	*/


struct			/* structure for gathering statistics*/
{
	int	m_udtime[2];	/* time of last update into log */
	int	m_hw_ops[14];	/* host write opcode counter */
	int	m_hr_ops[14];	/* host read    "      "     */
	int	m_iw_ops[9];	/* imp  write   "      "     */
	int	m_ir_ops[11];	/*  "   read    "      "     */
	int	m_unmcls;	/* counts unmatched closes */
	int	m_refrfc;	/* counts rfc's we refuse */
	int	m_ucls;		/* counts unmatched cls's */
}
measure;

