#include "transaction.h"
#include "debug.h"
#include "csapp.h"
/*
 * Initialize the transaction manager.
 */
void trans_init(void) {
	debug("Initialize transaction manager");
    trans_list.id = 0;
    trans_list.next = &trans_list;
    trans_list.prev = &trans_list;
    pthread_mutex_init(&trans_list.mutex,NULL);
    sem_init(&trans_list.sem, 0, 0);
}

/*
 * Finalize the transaction manager.
 */
void trans_fini(void) {
	debug("Finalize transaction manager");
}

/*
 * Create a new transaction.
 *
 * @return  A pointer to the new transaction (with reference count 1)
 * is returned if creation is successful, otherwise NULL is returned.
 */
TRANSACTION *trans_create(void) {
	TRANSACTION *new_trans = Malloc(sizeof(TRANSACTION));
	//initialize values
	new_trans->id = trans_list.id;
	trans_list.id++;
	new_trans->refcnt = 1;
	new_trans->status = TRANS_PENDING;
	new_trans->depends = NULL;
	new_trans->waitcnt = 0;
	if (pthread_mutex_init(&new_trans->mutex,NULL) == -1 ) {
		return NULL;
	}
	if (sem_init(&new_trans->sem, 0, 0) == -1 ) {
		return NULL;
	}
	new_trans->next = &trans_list;
    new_trans->prev = trans_list.prev;
    trans_list.prev->next = new_trans;
    trans_list.prev = new_trans;
	debug("Create new transaction %d",new_trans->id);
	return new_trans;
}

/*
 * Increase the reference count on a transaction.
 *
 * @param tp  The transaction.
 * @param why  Short phrase explaining the purpose of the increase.
 * @return  The transaction pointer passed as the argument.
 */
TRANSACTION *trans_ref(TRANSACTION *tp, char *why) {
	pthread_mutex_lock(&tp->mutex);
	tp->refcnt += 1;
	debug("Increase ref count of transaction %p to %d, for %s",tp,tp->refcnt, why);
	pthread_mutex_unlock(&tp->mutex);
	return tp;
}

/*
 * Decrease the reference count on a transaction.
 * If the reference count reaches zero, the transaction is freed.
 *
 * @param tp  The transaction.
 * @param why  Short phrase explaining the purpose of the decrease.
 */
void trans_unref(TRANSACTION *tp, char *why) {
	pthread_mutex_lock(&tp->mutex);
	tp->refcnt -= 1;
	debug("Decrease ref count of transaction %d to %d, for %s",tp->id,tp->refcnt, why);
	if (tp->refcnt == 0) {
		//get rid of dependency
		if (tp->depends != NULL) {
			debug("Release dependency of transaction %d", tp->id);
			DEPENDENCY *temp = tp->depends;
			while (temp != NULL) {
				DEPENDENCY *wow = temp;
				if (temp->trans->refcnt != 0) {
					trans_unref(temp->trans, "unref dependency in transaction");
				}
				temp = temp->next;
				wow->trans = NULL;
				wow->next = NULL;
				Free(wow);
			}
		}

		//set trans list
		tp->prev->next = tp->next;
		tp->next->prev = tp->prev;

		//get rid of mutex stuff
		pthread_mutex_unlock(&tp->mutex);
		pthread_mutex_destroy(&tp->mutex);
		sem_destroy(&tp->sem);

		Free(tp);
		return;
	}
	pthread_mutex_unlock(&tp->mutex);
}

/*
 * Add a transaction to the dependency set for this transaction.
 *
 * @param tp  The transaction to which the dependency is being added.
 * @param dtp  The transaction that is being added to the dependency set.
 */
void trans_add_dependency(TRANSACTION *tp, TRANSACTION *dtp) {
	pthread_mutex_lock(&tp->mutex);
	DEPENDENCY *temp = tp->depends;
	//check dup
	if (temp != NULL) {
		//checks if temp is next so doesn't check current
		while(temp->next != NULL) {
			if (temp->trans == dtp) {
				pthread_mutex_unlock(&tp->mutex);
				return;
			}
			temp = temp->next;
		}
		if (temp->trans == dtp) {
			pthread_mutex_unlock(&tp->mutex);
			return;
		}
	}
	DEPENDENCY *new_dep = Malloc(sizeof(DEPENDENCY));
	//if null (empty) make new
	if (tp->depends == NULL) {
		new_dep->trans = dtp;
		new_dep->next = NULL;
		tp->depends = new_dep;
	    trans_ref(dtp, "add dependency");
		pthread_mutex_unlock(&tp->mutex);
		debug("Make transaction %d dependent on transaction %d",tp->id, dtp->id);
		return;
	}
	//if no duplicate but not empty
	temp->next = new_dep;
	new_dep->trans = dtp;
	new_dep->next = NULL;
	debug("Make transaction %d dependent on transaction %d",tp->id, dtp->id);
    trans_ref(dtp, "add dependency");
	pthread_mutex_unlock(&tp->mutex);
}

/*
 * Try to commit a transaction.  Committing a transaction requires waiting
 * for all transactions in its dependency set to either commit or abort.
 * If any transaction in the dependency set abort, then the dependent
 * transaction must also abort.  If all transactions in the dependency set
 * commit, then the dependent transaction may also commit.
 *
 * In all cases, this function consumes a single reference to the transaction
 * object.
 *
 * @param tp  The transaction to be committed.
 * @return  The final status of the transaction: either TRANS_ABORTED,
 * or TRANS_COMMITTED.
 */
TRANS_STATUS trans_commit(TRANSACTION *tp) {
	debug("Transaction %d trying to commit",tp->id);
	//check aborted first
	if(tp->status==TRANS_ABORTED) {
        debug("Transaction already aborted");
        return TRANS_ABORTED;
    }
    if(tp->status==TRANS_COMMITTED) {
        debug("Transaction already committed");
        return TRANS_COMMITTED;
    }
    // tp->status = TRANS_COMMITTED;
	//if there is dependency
	if (tp->depends != NULL) {
		debug("GOING INTO COMMITING WITH DEPENDENCY");
		DEPENDENCY *temp = tp->depends;
		while (temp!=NULL) {
			TRANSACTION *iter = temp->trans;
			if (iter->status == TRANS_ABORTED) {
				trans_ref(tp, "dependency aborted");
				return trans_abort(tp);
			}
			pthread_mutex_lock(&tp->mutex);
			debug("Transaction %d waiting for dependency %d",tp->id, iter->id);
			iter->waitcnt++;
			pthread_mutex_unlock(&tp->mutex);
			debug("before sem_wait");
			sem_wait(&iter->sem);
			// if (iter->status == TRANS_ABORTED) {
			// 	trans_ref(iter, "dependency aborted");
			// 	return trans_abort(iter);
			// }
			debug("after sem_wait");
			temp = temp->next;
		}
	}
	debug("Starting Waitcnt: %d", tp->waitcnt);
	// DEPENDENCY *temp = tp->depends;
	// while (temp!= NULL) {
	// 	char *ex = "unref dependency in transaction";
	// 	trans_unref(temp->trans, ex);
	// 	temp = temp->next;
	// }
	pthread_mutex_lock(&tp->mutex);
    while(tp->waitcnt != 0) {
        sem_post(&tp->sem);
        tp->waitcnt--;
        debug("Waitcnt: %d", tp->waitcnt);
    }
    pthread_mutex_unlock(&tp->mutex);
    DEPENDENCY* temp=tp->depends;
	while(temp!=NULL){
		if(temp->trans->status==TRANS_ABORTED){
			// tp->status=TRANS_ABORTED;
			trans_ref(tp, "dependency aborted");
			return trans_abort(tp);
			// break;
		}
		temp=temp->next;
	}
    //no dependency
    trans_unref(tp,"commiting transaction");
    return TRANS_COMMITTED;
}

/*
 * Abort a transaction.  If the transaction has already committed, it is
 * a fatal error and the program crashes.  If the transaction has already
 * aborted, no change is made to its state.  If the transaction is pending,
 * then it is set to the aborted state, and any transactions dependent on
 * this transaction must also abort.
 *
 * In all cases, this function consumes a single reference to the transaction
 * object.
 *
 * @param tp  The transaction to be aborted.
 * @return  TRANS_ABORTED.
 */
TRANS_STATUS trans_abort(TRANSACTION *tp) {
	debug("Aboring transaction %d",tp->id);
    if(trans_get_status(tp) == TRANS_COMMITTED) {
        debug("Transaction already commited");
        abort();
    }
    if(trans_get_status(tp) == TRANS_ABORTED) {
        debug("Transaction already aborted");
        return TRANS_ABORTED;
    }
    tp->status = TRANS_ABORTED;
    debug("Waitcnt: %d",tp->waitcnt);
    pthread_mutex_lock(&tp->mutex);
    while(tp->waitcnt != 0) {
        sem_post(&tp->sem);
        tp->waitcnt--;
    }
    pthread_mutex_unlock(&tp->mutex);
    debug("Transaction %d has aborted",tp->id);
    // DEPENDENCY *temp = tp->depends;
	// while (temp!= NULL) {
	// 	char *ex = "unref dependency in transaction";
	// 	trans_unref(temp->trans, ex);
	// 	temp = temp->next;
	// }
    trans_unref(tp,"aborting transaction");
    return TRANS_ABORTED;
}

/*
 * Get the current status of a transaction.
 * If the value returned is TRANS_PENDING, then we learn nothing,
 * because unless we are holding the transaction mutex the transaction
 * could be aborted at any time.  However, if the value returned is
 * either TRANS_COMMITTED or TRANS_ABORTED, then that value is the
 * stable final status of the transaction.
 *
 * @param tp  The transaction.
 * @return  The status of the transaction, as it was at the time of call.
 */
TRANS_STATUS trans_get_status(TRANSACTION *tp) {
	return tp->status;
}

/*
 * Print information about a transaction to stderr.
 * No locking is performed, so this is not thread-safe.
 * This should only be used for debugging.
 *
 * @param tp  The transaction to be shown.
 */
void trans_show(TRANSACTION *tp) {
	int dep = 0;
	if (tp->depends != NULL) {
		dep = 1;
	}
	fprintf(stderr,"Transaction id:%d, status:%d, refcnt:%d, has_dependency:%d\n",tp->id,tp->status,tp->refcnt, dep);
}

/*
 * Print information about all transactions to stderr.
 * No locking is performed, so this is not thread-safe.
 * This should only be used for debugging.
 */
void trans_show_all(void) {
	TRANSACTION *tp = trans_list.next;
	while (tp->next != &trans_list){
        trans_show(tp);
        tp = tp->next;
    }
}