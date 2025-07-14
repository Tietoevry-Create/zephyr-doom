/*
 * Copyright (c) 2014 - 2017, Nordic Semiconductor ASA
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be
 * reverse engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @defgroup nrf_error SoftDevice Global Error Codes
 * @{
 * @brief Global Error definitions
 */

/* Header guard */
#ifndef NRF_ERROR_H__
#define NRF_ERROR_H__

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @defgroup NRF_ERRORS_BASE Error Codes Base number definitions
 * @{
 */
#define NRF_ERROR_SDM_BASE_NUM (0x1000) ///< SDM error base
#define NRF_ERROR_SOC_BASE_NUM (0x2000) ///< SoC error base
#define NRF_ERROR_STK_BASE_NUM (0x3000) ///< STK error base
/** @} */

    enum nrfx_err_t
    {
        NRF_SUCCESS = (0x0BAD0000 + 0),
        NRF_ERROR_INTERNAL = (0x0BAD0000 + 1),
        NRF_ERROR_NO_MEM = (0x0BAD0000 + 2),
        NRF_ERROR_NOT_SUPPORTED = (0x0BAD0000 + 3),
        NRF_ERROR_INVALID_PARAM = (0x0BAD0000 + 4),
        NRF_ERROR_INVALID_STATE = (0x0BAD0000 + 5),
        NRF_ERROR_INVALID_LENGTH = (0x0BAD0000 + 6),
        NRF_ERROR_TIMEOUT = (0x0BAD0000 + 7),
        NRF_ERROR_FORBIDDEN = (0x0BAD0000 + 8),
        NRF_ERROR_NULL = (0x0BAD0000 + 9),
        NRF_ERROR_INVALID_ADDR = (0x0BAD0000 + 10),
        NRF_ERROR_BUSY = (0x0BAD0000 + 11),
        NRF_ERROR_ALREADY_INITIALIZED = (0x0BAD0000 + 12),
        NRF_ERROR_DRV_TWI_ERR_OVERRUN = ((0x0BAD0000 + 0x10000) + 0),
        NRF_ERROR_DRV_TWI_ERR_ANACK = ((0x0BAD0000 + 0x10000) + 1),
        NRF_ERROR_DRV_TWI_ERR_DNACK = ((0x0BAD0000 + 0x10000) + 2)
    };

#ifdef __cplusplus
}
#endif

#endif /* NRF_ERROR_H__ */

/** @} */
