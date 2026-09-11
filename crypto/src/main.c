/*
 * Copyright (c) 2016 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Sample to illustrate the usage of crypto APIs. The sample plaintext
 * and ciphertexts used for crosschecking are from TinyCrypt.
 */

#include <stdlib.h>
#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/crypto/crypto.h>
#include <zephyr/sys/printk.h>
#define LOG_LEVEL CONFIG_CRYPTO_LOG_LEVEL

#if CONFIG_CRYPTO_MBEDTLS_SHIM
#define CRYPTO_DRV_NAME CONFIG_CRYPTO_MBEDTLS_SHIM_DRV_NAME
#elif DT_HAS_COMPAT_STATUS_OKAY(st_stm32_cryp)
#define CRYPTO_DEV_COMPAT st_stm32_cryp
#elif DT_HAS_COMPAT_STATUS_OKAY(st_stm32_aes)
#define CRYPTO_DEV_COMPAT st_stm32_aes
#elif DT_HAS_COMPAT_STATUS_OKAY(nxp_mcux_dcp)
#define CRYPTO_DEV_COMPAT nxp_mcux_dcp
#elif CONFIG_CRYPTO_NRF_ECB
#define CRYPTO_DEV_COMPAT nordic_nrf_ecb
#elif DT_HAS_COMPAT_STATUS_OKAY(renesas_smartbond_crypto)
#define CRYPTO_DEV_COMPAT renesas_smartbond_crypto
#else
#error "You need to enable one crypto device"
#endif

/*
int init(const struct device* dev) {
    if (!dev) {
        LOG_ERR("Device not found in device tree");
        return 1;
    }
    if (!device_is_ready(dev)) {
        LOG_ERR("Device not ready");
        return 1;
    }

    return 0;
}

const int PLAINTXT_LENGTH = 64;

const static uint8_t key[16] __aligned(32) = {
	0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6, 0xab, 0xf7, 0x15, 0x88,
	0x09, 0xcf, 0x4f, 0x3c
};

static uint8_t plaintext[64];

uint32_t cap_flags;

static void print_buffer_comparison(const uint8_t *wanted_result,
				    uint8_t *result, size_t length)
{
	int i, j;

	printk("Was waiting for: \n");

	for (i = 0, j = 1; i < length; i++, j++) {
		printk("0x%02x ", wanted_result[i]);

		if (j == 10) {
			printk("\n");
			j = 0;
		}
	}

	printk("\nBut got:\n");

	for (i = 0, j = 1; i < length; i++, j++) {
		printk("0x%02x ", result[i]);

		if (j == 10) {
			printk("\n");
			j = 0;
		}
	}

	printk("\n");
}

int validate_hw_compatibility(const struct device *dev)
{
	uint32_t flags = 0U;

	flags = crypto_query_hwcaps(dev);
	if ((flags & CAP_RAW_KEY) == 0U) {
		LOG_INF("Please provision the key separately "
			"as the module doesnt support a raw key");
		return -1;
	}

	if ((flags & CAP_SYNC_OPS) == 0U) {
		LOG_ERR("The app assumes sync semantics. "
		  "Please rewrite the app accordingly before proceeding");
		return -1;
	}

	if ((flags & CAP_SEPARATE_IO_BUFS) == 0U) {
		LOG_ERR("The app assumes distinct IO buffers. "
		"Please rewrite the app accordingly before proceeding");
		return -1;
	}

	cap_flags = CAP_RAW_KEY | CAP_SYNC_OPS | CAP_SEPARATE_IO_BUFS;

	return 0;

}

const int ECBTEXT_LENGTH = 16;


const uint8_t ecb_key[16] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
};
uint8_t ecb_plaintext[16];

bool ecb_mode(const struct device *dev)
{
	uint8_t encrypted[16] = {0};
	uint8_t decrypted[16] = {0};
	struct cipher_ctx ini = {
		.keylen = sizeof(ecb_key),
		.key.bit_stream = ecb_key,
		.flags = cap_flags,
	};
	struct cipher_pkt encrypt = {
		.in_buf = ecb_plaintext,
		.in_len = sizeof(ecb_plaintext),
		.out_buf_max = sizeof(encrypted),
		.out_buf = encrypted,
	};
	struct cipher_pkt decrypt = {
		.in_buf = encrypt.out_buf,
		.in_len = sizeof(encrypted),
		.out_buf = decrypted,
		.out_buf_max = sizeof(decrypted),
	};

	if (cipher_begin_session(dev, &ini, CRYPTO_CIPHER_ALGO_AES,
				 CRYPTO_CIPHER_MODE_ECB,
				 CRYPTO_CIPHER_OP_ENCRYPT)) {
		return false;
	}

	if (cipher_block_op(&ini, &encrypt)) {
		LOG_ERR("ECB mode ENCRYPT - Failed");
		cipher_free_session(dev, &ini);
		return false;
	}

	LOG_INF("Output length (encryption): %d", encrypt.out_len);

	cipher_free_session(dev, &ini);

	if (cipher_begin_session(dev, &ini, CRYPTO_CIPHER_ALGO_AES,
				 CRYPTO_CIPHER_MODE_ECB,
				 CRYPTO_CIPHER_OP_DECRYPT)) {
		return false;
	}

	if (cipher_block_op(&ini, &decrypt)) {
		LOG_ERR("ECB mode DECRYPT - Failed");
		cipher_free_session(dev, &ini);
		return false;
	}

	LOG_INF("Output length (decryption): %d", decrypt.out_len);

	if (memcmp(decrypt.out_buf, ecb_plaintext, sizeof(ecb_plaintext))) {
		LOG_ERR("ECB mode DECRYPT - Mismatch between plaintext and "
			    "decrypted cipher text");
		print_buffer_comparison(ecb_plaintext, decrypt.out_buf,
					sizeof(ecb_plaintext));
		cipher_free_session(dev, &ini);
		return false;
	}

	LOG_INF("ECB mode DECRYPT - Match");

	cipher_free_session(dev, &ini);
	return true;
}


// The noinline attribute prevents the compiler from merging this stack frame.
__attribute__((noinline)) void parse_packet_context(const uint8_t *data) {
    if (data[0] == 'C' && data[1] == 'T') {
        uint8_t version = data[2];
        uint8_t attr_len = data[3]; 
        
        uint8_t local_attr_buffer[16] = {0};
        
        if (version == 0x01) {
            // With a tiny stack frame, writing 40 bytes here will 
            // immediately smash this function's canary.
            memcpy(local_attr_buffer, &data[4], attr_len);
            
            if (local_attr_buffer[0] == 0xFF) {
                printk("Extended flags active.\n");
            }
        }
    }
}


bool cbc_mode(const struct device *dev)
{
	uint8_t local_attr_buffer[16] = {0};
	uint8_t encrypted[80] = {0};
	uint8_t decrypted[64] = {0};

    if (plaintext[0] == 'C' && plaintext[1] == 'T') {
        uint8_t version = plaintext[2];
        uint8_t attr_len = plaintext[3]; 
        
        if (version == 0x01) {
            // With a tiny stack frame, writing 40 bytes here will 
            // immediately smash this function's canary.
            memcpy(local_attr_buffer, &plaintext[4], attr_len);
            
            if (local_attr_buffer[0] == 0xFF) {
                printk("Extended flags active.\n");
            }
        }
    }

	// ---- INJECTED BUG FOR FUZZER EVALUATION ----
    //parse_packet_context(plaintext);
    // --------------------------------------------

	// uint8_t encrypted[80] = {0};
	// uint8_t decrypted[64] = {0};

	struct cipher_ctx ini = {
		.keylen = sizeof(key),
		.key.bit_stream = key,
		.flags = cap_flags,
	};
	struct cipher_pkt encrypt = {
		.in_buf = plaintext,
		.in_len = sizeof(plaintext),
		.out_buf_max = sizeof(encrypted),
		.out_buf = encrypted,
	};
	struct cipher_pkt decrypt = {
		.in_buf = encrypt.out_buf,
		.in_len = sizeof(encrypted),
		.out_buf = decrypted,
		.out_buf_max = sizeof(decrypted),
	};

	static uint8_t iv[16] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
	};

	if (cipher_begin_session(dev, &ini, CRYPTO_CIPHER_ALGO_AES,
				 CRYPTO_CIPHER_MODE_CBC,
				 CRYPTO_CIPHER_OP_ENCRYPT)) {
		return false;
	}

	if (cipher_cbc_op(&ini, &encrypt, iv)) {
		LOG_ERR("CBC mode ENCRYPT - Failed");
		cipher_free_session(dev, &ini);
		return false;
	}

	LOG_INF("Output length (encryption): %d", encrypt.out_len);

	cipher_free_session(dev, &ini);

	if (cipher_begin_session(dev, &ini, CRYPTO_CIPHER_ALGO_AES,
				 CRYPTO_CIPHER_MODE_CBC,
				 CRYPTO_CIPHER_OP_DECRYPT)) {
		return false;
	}


	if (cipher_cbc_op(&ini, &decrypt, encrypted)) {
		LOG_ERR("CBC mode DECRYPT - Failed");
		cipher_free_session(dev, &ini);
		return false;
	}

	LOG_INF("Output length (decryption): %d", decrypt.out_len);

	if (memcmp(decrypt.out_buf, plaintext, sizeof(plaintext))) {
		LOG_ERR("CBC mode DECRYPT - Mismatch between plaintext and "
			    "decrypted cipher text");
		print_buffer_comparison(plaintext, decrypt.out_buf,
					sizeof(plaintext));
		cipher_free_session(dev, &ini);
		return false;
	}

	LOG_INF("CBC mode DECRYPT - Match");
	cipher_free_session(dev, &ini);
	return true;
}

bool ctr_mode(const struct device *dev)
{
	uint8_t encrypted[64] = {0};
	uint8_t decrypted[64] = {0};
	struct cipher_ctx ini = {
		.keylen = sizeof(key),
		.key.bit_stream = key,
		.flags = cap_flags,
		.mode_params.ctr_info.ctr_len = 32,
	};
	struct cipher_pkt encrypt = {
		.in_buf = plaintext,
		.in_len = sizeof(plaintext),
		.out_buf_max = sizeof(encrypted),
		.out_buf = encrypted,
	};
	struct cipher_pkt decrypt = {
		.in_buf = encrypted,
		.in_len = sizeof(encrypted),
		.out_buf = decrypted,
		.out_buf_max = sizeof(decrypted),
	};
	uint8_t iv[12] = {
		0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
		0xf8, 0xf9, 0xfa, 0xfb
	};

	if (cipher_begin_session(dev, &ini, CRYPTO_CIPHER_ALGO_AES,
				 CRYPTO_CIPHER_MODE_CTR,
				 CRYPTO_CIPHER_OP_ENCRYPT)) {
		return false;
	}

	if (cipher_ctr_op(&ini, &encrypt, iv)) {
		LOG_ERR("CTR mode ENCRYPT - Failed");
		cipher_free_session(dev, &ini);
		return false;
	}

	LOG_INF("Output length (encryption): %d", encrypt.out_len);

	cipher_free_session(dev, &ini);

	if (cipher_begin_session(dev, &ini, CRYPTO_CIPHER_ALGO_AES,
				 CRYPTO_CIPHER_MODE_CTR,
				 CRYPTO_CIPHER_OP_DECRYPT)) {
		return false;
	}

	if (cipher_ctr_op(&ini, &decrypt, iv)) {
		LOG_ERR("CTR mode DECRYPT - Failed");
		cipher_free_session(dev, &ini);
		return false;
	}

	LOG_INF("Output length (decryption): %d", decrypt.out_len);

	if (memcmp(decrypt.out_buf, plaintext, sizeof(plaintext))) {
		LOG_ERR("CTR mode DECRYPT - Mismatch between plaintext "
			    "and decrypted cipher text");
		print_buffer_comparison(plaintext,
					decrypt.out_buf, sizeof(plaintext));
		cipher_free_session(dev, &ini);
		return false;
	}

	LOG_INF("CTR mode DECRYPT - Match");
	cipher_free_session(dev, &ini);
	return true;
}

const int CCM_LENGTH = 23;

const static uint8_t ccm_key[16] = {
	0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb,
	0xcc, 0xcd, 0xce, 0xcf
};
static uint8_t ccm_nonce[13] = {
	0x00, 0x00, 0x00, 0x03, 0x02, 0x01, 0x00, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4,
	0xa5
};
static uint8_t ccm_hdr[8] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
};
static uint8_t ccm_data[23];


bool ccm_mode(const struct device *dev)
{
	uint8_t encrypted[50];
	uint8_t decrypted[25];
	struct cipher_ctx ini = {
		.keylen = sizeof(ccm_key),
		.key.bit_stream = ccm_key,
		.mode_params.ccm_info = {
			.nonce_len = sizeof(ccm_nonce),
			.tag_len = 8,
		},
		.flags = cap_flags,
	};
	struct cipher_pkt encrypt = {
		.in_buf = ccm_data,
		.in_len = sizeof(ccm_data),
		.out_buf_max = sizeof(encrypted),
		.out_buf = encrypted,
	};
	struct cipher_aead_pkt ccm_op = {
		.ad = ccm_hdr,
		.ad_len = sizeof(ccm_hdr),
		.pkt = &encrypt,

		.tag = encrypted + sizeof(ccm_data),
	};
	struct cipher_pkt decrypt = {
		.in_buf = encrypted,
		.in_len = sizeof(ccm_data),
		.out_buf = decrypted,
		.out_buf_max = sizeof(decrypted),
	};

	if (cipher_begin_session(dev, &ini, CRYPTO_CIPHER_ALGO_AES,
				 CRYPTO_CIPHER_MODE_CCM,
				 CRYPTO_CIPHER_OP_ENCRYPT)) {
		return false;
	}

	ccm_op.pkt = &encrypt;
	if (cipher_ccm_op(&ini, &ccm_op, ccm_nonce)) {
		LOG_ERR("CCM mode ENCRYPT - Failed");
		cipher_free_session(dev, &ini);
		return false;
	}

	LOG_INF("Output length (encryption): %d", encrypt.out_len);

	cipher_free_session(dev, &ini);

	if (cipher_begin_session(dev, &ini, CRYPTO_CIPHER_ALGO_AES,
				 CRYPTO_CIPHER_MODE_CCM,
				 CRYPTO_CIPHER_OP_DECRYPT)) {
		return false;
	}

	ccm_op.pkt = &decrypt;
	if (cipher_ccm_op(&ini, &ccm_op, ccm_nonce)) {
		LOG_ERR("CCM mode DECRYPT - Failed");
		cipher_free_session(dev, &ini);
		return false;
	}

	LOG_INF("Output length (decryption): %d", decrypt.out_len);

	if (memcmp(decrypt.out_buf, ccm_data, sizeof(ccm_data))) {
		LOG_ERR("CCM mode DECRYPT - Mismatch between plaintext "
			"and decrypted cipher text");
		print_buffer_comparison(ccm_data,
					decrypt.out_buf, sizeof(ccm_data));
		cipher_free_session(dev, &ini);
		return false;
	}

	LOG_INF("CCM mode DECRYPT - Match");
	cipher_free_session(dev, &ini);
	return true;
}

const int GCM_LENGTH = 42;


const static uint8_t gcm_key[16] = {
	0x07, 0x1b, 0x11, 0x3b, 0x0c, 0xa7, 0x43, 0xfe, 0xcc, 0xcf, 0x3d, 0x05,
	0x1f, 0x73, 0x73, 0x82
};
static uint8_t gcm_nonce[12] = {
	0xf0, 0x76, 0x1e, 0x8d, 0xcd, 0x3d, 0x00, 0x01, 0x76, 0xd4, 0x57, 0xed
};
static uint8_t gcm_hdr[20] = {
	0xe2, 0x01, 0x06, 0xd7, 0xcd, 0x0d, 0xf0, 0x76, 0x1e, 0x8d, 0xcd, 0x3d,
	0x88, 0xe5, 0x4c, 0x2a, 0x76, 0xd4, 0x57, 0xed
};
static uint8_t gcm_data[42];

bool gcm_mode(const struct device *dev)
{
	uint8_t encrypted[60] = {0};
	uint8_t decrypted[44] = {0};
	struct cipher_ctx ini = {
		.keylen = sizeof(gcm_key),
		.key.bit_stream = gcm_key,
		.mode_params.gcm_info = {
			.nonce_len = sizeof(gcm_nonce),
			.tag_len = 16,
		},
		.flags = cap_flags,
	};
	struct cipher_pkt encrypt = {
		.in_buf = gcm_data,
		.in_len = sizeof(gcm_data),
		.out_buf_max = sizeof(encrypted),
		.out_buf = encrypted,
	};
	struct cipher_aead_pkt gcm_op = {
		.ad = gcm_hdr,
		.ad_len = sizeof(gcm_hdr),
		.pkt = &encrypt,

		.tag = encrypted + sizeof(gcm_data),
	};
	struct cipher_pkt decrypt = {
		.in_buf = encrypted,
		.in_len = sizeof(gcm_data),
		.out_buf = decrypted,
		.out_buf_max = sizeof(decrypted),
	};

	if (cipher_begin_session(dev, &ini, CRYPTO_CIPHER_ALGO_AES,
				 CRYPTO_CIPHER_MODE_GCM,
				 CRYPTO_CIPHER_OP_ENCRYPT)) {
		return false;
	}

	gcm_op.pkt = &encrypt;
	if (cipher_gcm_op(&ini, &gcm_op, gcm_nonce)) {
		LOG_ERR("GCM mode ENCRYPT - Failed");
		cipher_free_session(dev, &ini);
		return false;
	}

	LOG_INF("Output length (encryption): %d", encrypt.out_len);

	cipher_free_session(dev, &ini);

	if (cipher_begin_session(dev, &ini, CRYPTO_CIPHER_ALGO_AES,
				 CRYPTO_CIPHER_MODE_GCM,
				 CRYPTO_CIPHER_OP_DECRYPT)) {
		return false;
	}

	gcm_op.pkt = &decrypt;
	if (cipher_gcm_op(&ini, &gcm_op, gcm_nonce)) {
		LOG_ERR("GCM mode DECRYPT - Failed");
		cipher_free_session(dev, &ini);
		return false;
	}

	LOG_INF("Output length (decryption): %d", decrypt.out_len);

	if (memcmp(decrypt.out_buf, gcm_data, sizeof(gcm_data))) {
		LOG_ERR("GCM mode DECRYPT - Mismatch between plaintext "
			"and decrypted cipher text");
		print_buffer_comparison(gcm_data,
					decrypt.out_buf, sizeof(gcm_data));
		cipher_free_session(dev, &ini);
		return false;
	}

	LOG_INF("GCM mode DECRYPT - Match");
	cipher_free_session(dev, &ini);
	return true;
}

struct mode_test {
	const char *mode;
	bool (*mode_func)(const struct device *dev);
};


int main(void)
{
#ifdef CRYPTO_DRV_NAME
	const struct device *dev = device_get_binding(CRYPTO_DRV_NAME);

	if (!dev) {
		LOG_ERR("%s pseudo device not found", CRYPTO_DRV_NAME);
		return 0;
	}
#else
	const struct device *const dev = DEVICE_DT_GET_ONE(CRYPTO_DEV_COMPAT);

	if (!device_is_ready(dev)) {
		LOG_ERR("Crypto device is not ready\n");
		return 0;
	}
#endif


	const struct mode_test modes[] = {
		{ .mode = "ECB Mode", .mode_func = ecb_mode },
		{ .mode = "CBC Mode", .mode_func = cbc_mode },
		{ .mode = "CCM Mode", .mode_func = ccm_mode },
		{ .mode = "GCM Mode", .mode_func = gcm_mode },
		{ },
	};
	int i;

	if (validate_hw_compatibility(dev)) {
		LOG_ERR("Incompatible h/w");
		return 1;
	}

	LOG_INF("Cipher Sample");

	for (i = 0; modes[i].mode; i++) {
		LOG_INF("%s", modes[i].mode);
		if(!modes[i].mode_func(dev)){
			return 1;
		}
	}
    return 0;
}

*/

int main(void)
{
    printk("Hello world");
	return 0;
}