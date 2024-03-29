#include "data.h"
#include "csapp.h"
#include "debug.h"

/*
 * Create a blob with given content and size.
 * The content is copied, rather than shared with the caller.
 * The returned blob has one reference, which becomes the caller's
 * responsibility.
 *
 * @param content  The content of the blob.
 * @param size  The size in bytes of the content.
 * @return  The new blob, which has reference count 1.
*/
BLOB *blob_create(char *content, size_t size) {
	BLOB *new = (BLOB *)Malloc(sizeof(BLOB));
	new->content = strndup(content,size);
	new->prefix = strndup(content,size);
	new->refcnt = 1;
	new->size = size;
	pthread_mutex_init(&new->mutex, NULL);
	debug("Create new blob %p with content: %s, size: %ld", new, new->content, size);
	return new;
}

/*
 * Increase the reference count on a blob.
 *
 * @param bp  The blob.
 * @param why  Short phrase explaining the purpose of the increase.
 * @return  The blob pointer passed as the argument.
 */
BLOB *blob_ref(BLOB *bp, char *why) {
	pthread_mutex_lock(&bp->mutex);
	bp->refcnt += 1;
	debug("Increase ref count of blob %p to %d, for %s",bp, bp->refcnt, why);
	pthread_mutex_unlock(&bp->mutex);
	return bp;
}

/*
 * Decrease the reference count on a blob.
 * If the reference count reaches zero, the blob is freed.
 *
 * @param bp  The blob.
 * @param why  Short phrase explaining the purpose of the decrease.
 */
void blob_unref(BLOB *bp, char *why) {
	pthread_mutex_lock(&bp->mutex);
	bp->refcnt -= 1;
	debug("Decrease ref count of blob %p to %d, for %s", bp, bp->refcnt, why);
	pthread_mutex_unlock(&bp->mutex);
	if (bp->refcnt == 0) {
		debug("Free blob %p", bp);
		Free(bp->content);
		Free(bp->prefix);
		Free(bp);
	}
	return;
}

/*
 * Compare two blobs for equality of their content.
 *
 * @param bp1  The first blob.
 * @param bp2  The second blob.
 * @return 0 if the blobs have equal content, nonzero otherwise.
 */
int blob_compare(BLOB *bp1, BLOB *bp2) {
	if (bp1 == NULL || bp2 == NULL) {
		return -1;
	}
	if (bp1->size != bp2->size) {
		return -1;
	}
	return strcmp(bp1->content, bp2->content);
}

/*
 * Hash function for hashing the content of a blob.
 * 
 * @param bp  The blob.
 * @return  Hash of the blob.
 */
int blob_hash(BLOB *bp) {
	char *string = bp->content;
	int hash = 0;
	for (int i=0; i<strlen(string); i++) {
		hash += string[i] * (i+1);
	}
	debug("!!!Hash value: %d",hash%FD_SETSIZE);
	return hash%FD_SETSIZE;
}

/*
 * Create a key from a blob.
 * The key inherits the caller's reference to the blob.
 *
 * @param bp  The blob.
 * @return  the newly created key.
 */
KEY *key_create(BLOB *bp){
	KEY* key = (KEY *)Malloc(sizeof(KEY));
    key->hash = blob_hash(bp);
    key->blob = bp;
    debug("Create key %p from blob %p [%s]",key,bp,bp->content);
    return key;
}

/*
 * Dispose of a key, decreasing the reference count of the contained blob.
 * A key must be disposed of only once and must not be referred to again
 * after it has been disposed.
 *
 * @param kp  The key.
 */
void key_dispose(KEY *kp) {
    blob_unref(kp->blob,"dispose key");
    free(kp);
}

/*
 * Compare two keys for equality.
 *
 * @param kp1  The first key.
 * @param kp2  The second key.
 * @return  0 if the keys are equal, otherwise nonzero.
 */
int key_compare(KEY *kp1, KEY *kp2){
	if (kp1 == NULL || kp2 == NULL) {
		return -1;
	}
	if (kp1->hash != kp2->hash) {
		return -1;
	}
	return (blob_compare(kp1->blob,kp2->blob));
}

/*
 * Create a version of a blob for a specified creator transaction.
 * The version inherits the caller's reference to the blob.
 * The reference count of the creator transaction is increased to
 * account for the reference that is stored in the version.
 *
 * @param tp  The creator transaction.
 * @param bp  The blob.
 * @return  The newly created version.
 */
VERSION *version_create(TRANSACTION *tp, BLOB *bp) {
	debug("Creating version for transaction %p", tp);
	VERSION *new = Malloc(sizeof(VERSION));
	new->creator = tp;
	new->blob =  bp;
	new->next = NULL;
	new->prev = NULL;
	trans_ref(tp, "create version");
	return new;
}

/*
 * Dispose of a version, decreasing the reference count of the
 * creator transaction and contained blob.  A version must be
 * disposed of only once and must not be referred to again once
 * it has been disposed.
 *
 * @param vp  The version to be disposed.
 */
void version_dispose(VERSION *vp) {
	debug("Disposing version %p", vp);
    trans_unref(vp->creator,"dispose version");
    if (vp->blob != NULL) {
    	blob_unref(vp->blob,"dispose version");
    }
    Free(vp);
}