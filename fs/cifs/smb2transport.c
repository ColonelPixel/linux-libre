/*
 *   fs/cifs/smb2transport.c
 *
 *   Copyright (C) International Business Machines  Corp., 2002, 2011
 *                 Etersoft, 2012
 *   Author(s): Steve French (sfrench@us.ibm.com)
 *              Jeremy Allison (jra@samba.org) 2006
 *              Pavel Shilovsky (pshilovsky@samba.org) 2012
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published
 *   by the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/fs.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/net.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <asm/processor.h>
#include <linux/mempool.h>
#include <linux/highmem.h>
#include "smb2pdu.h"
#include "cifsglob.h"
#include "cifsproto.h"
#include "smb2proto.h"
#include "cifs_debug.h"
#include "smb2status.h"
#include "smb2glob.h"

static int
smb2_crypto_shash_allocate(struct TCP_Server_Info *server)
{
	int rc;
	unsigned int size;

	if (server->secmech.sdeschmacsha256 != NULL)
		return 0; /* already allocated */

	server->secmech.hmacsha256 = crypto_alloc_shash("hmac(sha256)", 0, 0);
	if (IS_ERR(server->secmech.hmacsha256)) {
		cifs_dbg(VFS, "could not allocate crypto hmacsha256\n");
		rc = PTR_ERR(server->secmech.hmacsha256);
		server->secmech.hmacsha256 = NULL;
		return rc;
	}

	size = sizeof(struct shash_desc) +
			crypto_shash_descsize(server->secmech.hmacsha256);
	server->secmech.sdeschmacsha256 = kmalloc(size, GFP_KERNEL);
	if (!server->secmech.sdeschmacsha256) {
		crypto_free_shash(server->secmech.hmacsha256);
		server->secmech.hmacsha256 = NULL;
		return -ENOMEM;
	}
	server->secmech.sdeschmacsha256->shash.tfm = server->secmech.hmacsha256;
	server->secmech.sdeschmacsha256->shash.flags = 0x0;

	return 0;
}

static int
smb3_crypto_shash_allocate(struct TCP_Server_Info *server)
{
	unsigned int size;
	int rc;

	if (server->secmech.sdesccmacaes != NULL)
		return 0;  /* already allocated */

	rc = smb2_crypto_shash_allocate(server);
	if (rc)
		return rc;

	server->secmech.cmacaes = crypto_alloc_shash("cmac(aes)", 0, 0);
	if (IS_ERR(server->secmech.cmacaes)) {
		cifs_dbg(VFS, "could not allocate crypto cmac-aes");
		kfree(server->secmech.sdeschmacsha256);
		server->secmech.sdeschmacsha256 = NULL;
		crypto_free_shash(server->secmech.hmacsha256);
		server->secmech.hmacsha256 = NULL;
		rc = PTR_ERR(server->secmech.cmacaes);
		server->secmech.cmacaes = NULL;
		return rc;
	}

	size = sizeof(struct shash_desc) +
			crypto_shash_descsize(server->secmech.cmacaes);
	server->secmech.sdesccmacaes = kmalloc(size, GFP_KERNEL);
	if (!server->secmech.sdesccmacaes) {
		cifs_dbg(VFS, "%s: Can't alloc cmacaes\n", __func__);
		kfree(server->secmech.sdeschmacsha256);
		server->secmech.sdeschmacsha256 = NULL;
		crypto_free_shash(server->secmech.hmacsha256);
		crypto_free_shash(server->secmech.cmacaes);
		server->secmech.hmacsha256 = NULL;
		server->secmech.cmacaes = NULL;
		return -ENOMEM;
	}
	server->secmech.sdesccmacaes->shash.tfm = server->secmech.cmacaes;
	server->secmech.sdesccmacaes->shash.flags = 0x0;

	return 0;
}

static struct cifs_ses *
smb2_find_smb_ses_unlocked(struct TCP_Server_Info *server, __u64 ses_id)
{
	struct cifs_ses *ses;

	list_for_each_entry(ses, &server->smb_ses_list, smb_ses_list) {
		if (ses->Suid != ses_id)
			continue;
		return ses;
	}

	return NULL;
}

struct cifs_ses *
smb2_find_smb_ses(struct TCP_Server_Info *server, __u64 ses_id)
{
	struct cifs_ses *ses;

	spin_lock(&cifs_tcp_ses_lock);
	ses = smb2_find_smb_ses_unlocked(server, ses_id);
	spin_unlock(&cifs_tcp_ses_lock);

	return ses;
}

static struct cifs_tcon *
smb2_find_smb_sess_tcon_unlocked(struct cifs_ses *ses, __u32  tid)
{
	struct cifs_tcon *tcon;

	list_for_each_entry(tcon, &ses->tcon_list, tcon_list) {
		if (tcon->tid != tid)
			continue;
		++tcon->tc_count;
		return tcon;
	}

	return NULL;
}

/*
 * Obtain tcon corresponding to the tid in the given
 * cifs_ses
 */

struct cifs_tcon *
smb2_find_smb_tcon(struct TCP_Server_Info *server, __u64 ses_id, __u32 tid)
{
	struct cifs_ses *ses;
	struct cifs_tcon *tcon;

	spin_lock(&cifs_tcp_ses_lock);
	ses = smb2_find_smb_ses_unlocked(server, ses_id);
	if (!ses) {
		spin_unlock(&cifs_tcp_ses_lock);
		return NULL;
	}
	tcon = smb2_find_smb_sess_tcon_unlocked(ses, tid);
	spin_unlock(&cifs_tcp_ses_lock);

	return tcon;
}

int
smb2_calc_signature(struct smb_rqst *rqst, struct TCP_Server_Info *server)
{
	int rc;
	unsigned char smb2_signature[SMB2_HMACSHA256_SIZE];
	unsigned char *sigptr = smb2_signature;
	struct kvec *iov = rqst->rq_iov;
	struct smb2_hdr *smb2_pdu = (struct smb2_hdr *)iov[0].iov_base;
	struct cifs_ses *ses;

	ses = smb2_find_smb_ses(server, smb2_pdu->SessionId);
	if (!ses) {
		cifs_dbg(VFS, "%s: Could not find session\n", __func__);
		return 0;
	}

	memset(smb2_signature, 0x0, SMB2_HMACSHA256_SIZE);
	memset(smb2_pdu->Signature, 0x0, SMB2_SIGNATURE_SIZE);

	rc = smb2_crypto_shash_allocate(server);
	if (rc) {
		cifs_dbg(VFS, "%s: shah256 alloc failed\n", __func__);
		return rc;
	}

	rc = crypto_shash_setkey(server->secmech.hmacsha256,
		ses->auth_key.response, SMB2_NTLMV2_SESSKEY_SIZE);
	if (rc) {
		cifs_dbg(VFS, "%s: Could not update with response\n", __func__);
		return rc;
	}

	rc = crypto_shash_init(&server->secmech.sdeschmacsha256->shash);
	if (rc) {
		cifs_dbg(VFS, "%s: Could not init sha256", __func__);
		return rc;
	}

	rc = __cifs_calc_signature(rqst, server, sigptr,
		&server->secmech.sdeschmacsha256->shash);

	if (!rc)
		memcpy(smb2_pdu->Signature, sigptr, SMB2_SIGNATURE_SIZE);

	return rc;
}

static int generate_key(struct cifs_ses *ses, struct kvec label,
			struct kvec context, __u8 *key, unsigned int key_size)
{
	unsigned char zero = 0x0;
	__u8 i[4] = {0, 0, 0, 1};
	__u8 L[4] = {0, 0, 0, 128};
	int rc = 0;
	unsigned char prfhash[SMB2_HMACSHA256_SIZE];
	unsigned char *hashptr = prfhash;

	memset(prfhash, 0x0, SMB2_HMACSHA256_SIZE);
	memset(key, 0x0, key_size);

	rc = smb3_crypto_shash_allocate(ses->server);
	if (rc) {
		cifs_dbg(VFS, "%s: crypto alloc failed\n", __func__);
		goto smb3signkey_ret;
	}

	rc = crypto_shash_setkey(ses->server->secmech.hmacsha256,
		ses->auth_key.response, SMB2_NTLMV2_SESSKEY_SIZE);
	if (rc) {
		cifs_dbg(VFS, "%s: Could not set with session key\n", __func__);
		goto smb3signkey_ret;
	}

	rc = crypto_shash_init(&ses->server->secmech.sdeschmacsha256->shash);
	if (rc) {
		cifs_dbg(VFS, "%s: Could not init sign hmac\n", __func__);
		goto smb3signkey_ret;
	}

	rc = crypto_shash_update(&ses->server->secmech.sdeschmacsha256->shash,
				i, 4);
	if (rc) {
		cifs_dbg(VFS, "%s: Could not update with n\n", __func__);
		goto smb3signkey_ret;
	}

	rc = crypto_shash_update(&ses->server->secmech.sdeschmacsha256->shash,
				label.iov_base, label.iov_len);
	if (rc) {
		cifs_dbg(VFS, "%s: Could not update with label\n", __func__);
		goto smb3signkey_ret;
	}

	rc = crypto_shash_update(&ses->server->secmech.sdeschmacsha256->shash,
				&zero, 1);
	if (rc) {
		cifs_dbg(VFS, "%s: Could not update with zero\n", __func__);
		goto smb3signkey_ret;
	}

	rc = crypto_shash_update(&ses->server->secmech.sdeschmacsha256->shash,
				context.iov_base, context.iov_len);
	if (rc) {
		cifs_dbg(VFS, "%s: Could not update with context\n", __func__);
		goto smb3signkey_ret;
	}

	rc = crypto_shash_update(&ses->server->secmech.sdeschmacsha256->shash,
				L, 4);
	if (rc) {
		cifs_dbg(VFS, "%s: Could not update with L\n", __func__);
		goto smb3signkey_ret;
	}

	rc = crypto_shash_final(&ses->server->secmech.sdeschmacsha256->shash,
				hashptr);
	if (rc) {
		cifs_dbg(VFS, "%s: Could not generate sha256 hash\n", __func__);
		goto smb3signkey_ret;
	}

	memcpy(key, hashptr, key_size);

smb3signkey_ret:
	return rc;
}

struct derivation {
	struct kvec label;
	struct kvec context;
};

struct derivation_triplet {
	struct derivation signing;
	struct derivation encryption;
	struct derivation decryption;
};

static int
generate_smb3signingkey(struct cifs_ses *ses,
			const struct derivation_triplet *ptriplet)
{
	int rc;

	rc = generate_key(ses, ptriplet->signing.label,
			  ptriplet->signing.context, ses->smb3signingkey,
			  SMB3_SIGN_KEY_SIZE);
	if (rc)
		return rc;

	rc = generate_key(ses, ptriplet->encryption.label,
			  ptriplet->encryption.context, ses->smb3encryptionkey,
			  SMB3_SIGN_KEY_SIZE);
	if (rc)
		return rc;

	return generate_key(ses, ptriplet->decryption.label,
			    ptriplet->decryption.context,
			    ses->smb3decryptionkey, SMB3_SIGN_KEY_SIZE);
}

int
generate_smb30signingkey(struct cifs_ses *ses)

{
	struct derivation_triplet triplet;
	struct derivation *d;

	d = &triplet.signing;
	d->label.iov_base = "SMB2AESCMAC";
	d->label.iov_len = 12;
	d->context.iov_base = "SmbSign";
	d->context.iov_len = 8;

	d = &triplet.encryption;
	d->label.iov_base = "SMB2AESCCM";
	d->label.iov_len = 11;
	d->context.iov_base = "ServerIn ";
	d->context.iov_len = 10;

	d = &triplet.decryption;
	d->label.iov_base = "SMB2AESCCM";
	d->label.iov_len = 11;
	d->context.iov_base = "ServerOut";
	d->context.iov_len = 10;

	return generate_smb3signingkey(ses, &triplet);
}

int
generate_smb311signingkey(struct cifs_ses *ses)

{
	struct derivation_triplet triplet;
	struct derivation *d;

	d = &triplet.signing;
	d->label.iov_base = "SMB2AESCMAC";
	d->label.iov_len = 12;
	d->context.iov_base = "SmbSign";
	d->context.iov_len = 8;

	d = &triplet.encryption;
	d->label.iov_base = "SMB2AESCCM";
	d->label.iov_len = 11;
	d->context.iov_base = "ServerIn ";
	d->context.iov_len = 10;

	d = &triplet.decryption;
	d->label.iov_base = "SMB2AESCCM";
	d->label.iov_len = 11;
	d->context.iov_base = "ServerOut";
	d->context.iov_len = 10;

	return generate_smb3signingkey(ses, &triplet);
}

int
smb3_calc_signature(struct smb_rqst *rqst, struct TCP_Server_Info *server)
{
	int rc = 0;
	unsigned char smb3_signature[SMB2_CMACAES_SIZE];
	unsigned char *sigptr = smb3_signature;
	struct kvec *iov = rqst->rq_iov;
	struct smb2_hdr *smb2_pdu = (struct smb2_hdr *)iov[0].iov_base;
	struct cifs_ses *ses;

	ses = smb2_find_smb_ses(server, smb2_pdu->SessionId);
	if (!ses) {
		cifs_dbg(VFS, "%s: Could not find session\n", __func__);
		return 0;
	}

	memset(smb3_signature, 0x0, SMB2_CMACAES_SIZE);
	memset(smb2_pdu->Signature, 0x0, SMB2_SIGNATURE_SIZE);

	rc = crypto_shash_setkey(server->secmech.cmacaes,
		ses->smb3signingkey, SMB2_CMACAES_SIZE);

	if (rc) {
		cifs_dbg(VFS, "%s: Could not set key for cmac aes\n", __func__);
		return rc;
	}

	/*
	 * we already allocate sdesccmacaes when we init smb3 signing key,
	 * so unlike smb2 case we do not have to check here if secmech are
	 * initialized
	 */
	rc = crypto_shash_init(&server->secmech.sdesccmacaes->shash);
	if (rc) {
		cifs_dbg(VFS, "%s: Could not init cmac aes\n", __func__);
		return rc;
	}
	
	rc = __cifs_calc_signature(rqst, server, sigptr,
				   &server->secmech.sdesccmacaes->shash);

	if (!rc)
		memcpy(smb2_pdu->Signature, sigptr, SMB2_SIGNATURE_SIZE);

	return rc;
}

/* must be called with server->srv_mutex held */
static int
smb2_sign_rqst(struct smb_rqst *rqst, struct TCP_Server_Info *server)
{
	int rc = 0;
	struct smb2_hdr *smb2_pdu = rqst->rq_iov[0].iov_base;

	if (!(smb2_pdu->Flags & SMB2_FLAGS_SIGNED) ||
	    server->tcpStatus == CifsNeedNegotiate)
		return rc;

	if (!server->session_estab) {
		strncpy(smb2_pdu->Signature, "BSRSPYL", 8);
		return rc;
	}

	rc = server->ops->calc_signature(rqst, server);

	return rc;
}

int
smb2_verify_signature(struct smb_rqst *rqst, struct TCP_Server_Info *server)
{
	unsigned int rc;
	char server_response_sig[16];
	struct smb2_hdr *smb2_pdu = (struct smb2_hdr *)rqst->rq_iov[0].iov_base;

	if ((smb2_pdu->Command == SMB2_NEGOTIATE) ||
	    (smb2_pdu->Command == SMB2_SESSION_SETUP) ||
	    (smb2_pdu->Command == SMB2_OPLOCK_BREAK) ||
	    (!server->session_estab))
		return 0;

	/*
	 * BB what if signatures are supposed to be on for session but
	 * server does not send one? BB
	 */

	/* Do not need to verify session setups with signature "BSRSPYL " */
	if (memcmp(smb2_pdu->Signature, "BSRSPYL ", 8) == 0)
		cifs_dbg(FYI, "dummy signature received for smb command 0x%x\n",
			 smb2_pdu->Command);

	/*
	 * Save off the origiginal signature so we can modify the smb and check
	 * our calculated signature against what the server sent.
	 */
	memcpy(server_response_sig, smb2_pdu->Signature, SMB2_SIGNATURE_SIZE);

	memset(smb2_pdu->Signature, 0, SMB2_SIGNATURE_SIZE);

	mutex_lock(&server->srv_mutex);
	rc = server->ops->calc_signature(rqst, server);
	mutex_unlock(&server->srv_mutex);

	if (rc)
		return rc;

	if (memcmp(server_response_sig, smb2_pdu->Signature,
		   SMB2_SIGNATURE_SIZE))
		return -EACCES;
	else
		return 0;
}

/*
 * Set message id for the request. Should be called after wait_for_free_request
 * and when srv_mutex is held.
 */
static inline void
smb2_seq_num_into_buf(struct TCP_Server_Info *server, struct smb2_hdr *hdr)
{
	unsigned int i, num = le16_to_cpu(hdr->CreditCharge);

	hdr->MessageId = get_next_mid64(server);
	/* skip message numbers according to CreditCharge field */
	for (i = 1; i < num; i++)
		get_next_mid(server);
}

static struct mid_q_entry *
smb2_mid_entry_alloc(const struct smb2_hdr *smb_buffer,
		     struct TCP_Server_Info *server)
{
	struct mid_q_entry *temp;

	if (server == NULL) {
		cifs_dbg(VFS, "Null TCP session in smb2_mid_entry_alloc\n");
		return NULL;
	}

	temp = mempool_alloc(cifs_mid_poolp, GFP_NOFS);
	if (temp == NULL)
		return temp;
	else {
		memset(temp, 0, sizeof(struct mid_q_entry));
		temp->mid = le64_to_cpu(smb_buffer->MessageId);
		temp->pid = current->pid;
		temp->command = smb_buffer->Command;	/* Always LE */
		temp->when_alloc = jiffies;
		temp->server = server;

		/*
		 * The default is for the mid to be synchronous, so the
		 * default callback just wakes up the current task.
		 */
		temp->callback = cifs_wake_up_task;
		temp->callback_data = current;
	}

	atomic_inc(&midCount);
	temp->mid_state = MID_REQUEST_ALLOCATED;
	return temp;
}

static int
smb2_get_mid_entry(struct cifs_ses *ses, struct smb2_hdr *buf,
		   struct mid_q_entry **mid)
{
	if (ses->server->tcpStatus == CifsExiting)
		return -ENOENT;

	if (ses->server->tcpStatus == CifsNeedReconnect) {
		cifs_dbg(FYI, "tcp session dead - return to caller to retry\n");
		return -EAGAIN;
	}

	if (ses->status == CifsNew) {
		if ((buf->Command != SMB2_SESSION_SETUP) &&
		    (buf->Command != SMB2_NEGOTIATE))
			return -EAGAIN;
		/* else ok - we are setting up session */
	}

	if (ses->status == CifsExiting) {
		if (buf->Command != SMB2_LOGOFF)
			return -EAGAIN;
		/* else ok - we are shutting down the session */
	}

	*mid = smb2_mid_entry_alloc(buf, ses->server);
	if (*mid == NULL)
		return -ENOMEM;
	spin_lock(&GlobalMid_Lock);
	list_add_tail(&(*mid)->qhead, &ses->server->pending_mid_q);
	spin_unlock(&GlobalMid_Lock);
	return 0;
}

int
smb2_check_receive(struct mid_q_entry *mid, struct TCP_Server_Info *server,
		   bool log_error)
{
	unsigned int len = get_rfc1002_length(mid->resp_buf);
	struct kvec iov;
	struct smb_rqst rqst = { .rq_iov = &iov,
				 .rq_nvec = 1 };

	iov.iov_base = (char *)mid->resp_buf;
	iov.iov_len = get_rfc1002_length(mid->resp_buf) + 4;

	dump_smb(mid->resp_buf, min_t(u32, 80, len));
	/* convert the length into a more usable form */
	if (len > 24 && server->sign) {
		int rc;

		rc = smb2_verify_signature(&rqst, server);
		if (rc)
			cifs_dbg(VFS, "SMB signature verification returned error = %d\n",
				 rc);
	}

	return map_smb2_to_linux_error(mid->resp_buf, log_error);
}

struct mid_q_entry *
smb2_setup_request(struct cifs_ses *ses, struct smb_rqst *rqst)
{
	int rc;
	struct smb2_hdr *hdr = (struct smb2_hdr *)rqst->rq_iov[0].iov_base;
	struct mid_q_entry *mid;

	smb2_seq_num_into_buf(ses->server, hdr);

	rc = smb2_get_mid_entry(ses, hdr, &mid);
	if (rc)
		return ERR_PTR(rc);
	rc = smb2_sign_rqst(rqst, ses->server);
	if (rc) {
		cifs_delete_mid(mid);
		return ERR_PTR(rc);
	}
	return mid;
}

struct mid_q_entry *
smb2_setup_async_request(struct TCP_Server_Info *server, struct smb_rqst *rqst)
{
	int rc;
	struct smb2_hdr *hdr = (struct smb2_hdr *)rqst->rq_iov[0].iov_base;
	struct mid_q_entry *mid;

	smb2_seq_num_into_buf(server, hdr);

	mid = smb2_mid_entry_alloc(hdr, server);
	if (mid == NULL)
		return ERR_PTR(-ENOMEM);

	rc = smb2_sign_rqst(rqst, server);
	if (rc) {
		DeleteMidQEntry(mid);
		return ERR_PTR(rc);
	}

	return mid;
}
