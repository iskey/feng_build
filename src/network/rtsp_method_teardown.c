/* *
 * This file is part of Feng
 *
 * Copyright (C) 2009 by LScube team <team@lscube.org>
 * See AUTHORS for more details
 *
 * feng is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * feng is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with feng; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * */

/** @file
 * @brief Contains TEARDOWN method and reply handlers
 */

#include "rtsp.h"

/**
 * RTSP TEARDOWN method handler
 * @param rtsp the buffer for which to handle the method
 * @param req The client request for the method
 * @todo trigger the release of rtp resources here
 */
void RTSP_teardown(ATTR_UNUSED RTSP_Client *rtsp,
                   RTSP_Request *req)
{
    if ( !rtsp_request_check_url(req) )
        return;

//    ev_async_send(rtsp->srv->loop, rtsp->ev_sig_disconnect);

    rtsp_quick_response(req, RTSP_Ok);
}
