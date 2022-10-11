        /* tcp machine predicates */

	/* acceptable ACK */

#define ack_ok(x, y) (!y->t_ack || (x->iss < y->t_ackno && y->t_ackno <= x->snd_hi))
 
	/* acceptable SYN */

#define syn_ok(x, y) (y->t_syn)

	/* ACK of local FIN */

#define ack_fin(x, y) (x->seq_fin > x->iss && y->t_ackno > x->seq_fin)

	/* receive buffer empty */

#define rcv_empty(x) (x->usr_abort || (x->t_ucb->uc_rbuf == NULL && x->t_rcv_next == x->t_rcv_prev))

