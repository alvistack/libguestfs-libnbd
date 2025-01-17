/* libnbd golang tests
 * Copyright (C) 2013-2021 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

package libnbd

import "syscall"
import "testing"

func Test610Error(t *testing.T) {
	h, err := Create()
	if err != nil {
		t.Fatalf("could not create handle: %s", err)
	}
	defer h.Close()

	/* This will always return an error because the handle is
	   not connected. */
	buf := make([]byte, 512)
	err = h.Pread(buf, 0, nil)
	if err == nil {
		t.Fatalf("expected an error from operation")
	} else if err.(*LibnbdError).Errno != syscall.ENOTCONN {
		t.Fatalf("unexpected error: %s", err)
	}
}
