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

import "fmt"
import "testing"

func Test300GetSize(t *testing.T) {
	expected := uint64(1048576)

	h, err := Create()
	if err != nil {
		t.Fatalf("could not create handle: %s", err)
	}
	defer h.Close()

	err = h.ConnectCommand([]string{
		"nbdkit", "-s", "--exit-with-parent", "-v", "null",
		fmt.Sprintf("size=%d", expected),
	})
	if err != nil {
		t.Fatalf("could not connect: %s", err)
	}
	actual, err := h.GetSize()
	if err != nil {
		t.Fatalf("%s", err)
	}
	if actual != expected {
		t.Fatalf("actual %d != expected %d", actual, expected)
	}
}
