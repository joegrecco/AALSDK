// Copyright(c) 2014, Altera Corporation
// All rights reserved.
// Copyright(c) 2007-2016, Intel Corporation
// All rights reserved.
//
// Redistribution  and  use  in source  and  binary  forms,  with  or  without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of  source code  must retain the  above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name  of Intel Corporation  nor the names of its contributors
//   may be used to  endorse or promote  products derived  from this  software
//   without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,  BUT NOT LIMITED TO,  THE
// IMPLIED WARRANTIES OF  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT  SHALL THE COPYRIGHT OWNER  OR CONTRIBUTORS BE
// LIABLE  FOR  ANY  DIRECT,  INDIRECT,  INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR
// CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT LIMITED  TO,  PROCUREMENT  OF
// SUBSTITUTE GOODS OR SERVICES;  LOSS OF USE,  DATA, OR PROFITS;  OR BUSINESS
// INTERRUPTION)  HOWEVER CAUSED  AND ON ANY THEORY  OF LIABILITY,  WHETHER IN
// CONTRACT,  STRICT LIABILITY,  OR TORT  (INCLUDING NEGLIGENCE  OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,  EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//****************************************************************************
/// @file mmlink_connection.cpp
/// @brief Basic AFU interaction.
/// @ingroup SigTap
/// @verbatim
/// Accelerator Abstraction Layer Sample Application
///
///    This application is for example purposes only.
///    It is not intended to represent a model for developing commercially-deployable applications.
///    It is designed to show working examples of the AAL programming model and APIs.
///
/// AUTHORS: Sadruta Chandrashekar, Intel Corporation.
///
/// HISTORY:
/// WHEN:          WHO:     WHAT:
/// 01/19/2016     SC       Initial version started based on Altera's sample code.@endverbatim
//****************************************************************************

#include <aalsdk/AAL.h>
#include <errno.h>
#include <string.h>
#include <sys/param.h>

//#include "printf.h"
#include "mmlink_connection.h"

using namespace std;
using namespace AAL;

// Convenience macros for printing messages and errors.
#ifdef MSG
# undef MSG
#endif // MSG
#define MSG(x) std::cout << __AAL_SHORT_FILE__ << ':' << __LINE__ << ':' << __AAL_FUNC__ << "() : " << x << std::endl
#ifdef ERR
# undef ERR
#endif // ERR
#define ERR(x) std::cerr << __AAL_SHORT_FILE__ << ':' << __LINE__ << ':' << __AAL_FUNC__ << "() **Error : " << x << std::endl

const char *mmlink_connection::UNKNOWN = "UNKNOWN\n";
const char *mmlink_connection::OK = "OK\n";

// return value:
//   0: everything A-OK
//   negative: error code
int mmlink_connection::handle_receive()
{
  int fail = 0;
  int size = 0;
  int conn = this->socket();

  int bytes_to_receive = m_bufsize - m_buf_end;
  if (bytes_to_receive == 0)
  {
    // No room for more data, so exit.
    return 0;
  }

  size = ::recv(conn, m_buf + m_buf_end, bytes_to_receive, 0);
  if (size < 0)
  {
    if (errno == EAGAIN || errno == EWOULDBLOCK)
    {
      // Nothing to do, but no error.
      fail = 0;
    }
    else
    {
      cerr << "error on socket " << conn << " : " << errno << " " << strerror(errno) << endl;
      fail = -errno;
    }
  }
  else if (size == 0)
  {
    fail = -1;
  }
  else
  {
    m_buf_end += size;
  }

  return fail;
}

size_t mmlink_connection::send(const char *msg, const size_t msg_len)
{
  size_t len;

  len = ::send(m_fd, msg, msg_len, 0);
  return len;
}

int mmlink_connection::handle_management()
{
  int i, start;
  size_t rem;
  int fail = 0;

  i = 0;
  start = 0;
  for (i = 0; i < m_buf_end; ++i)
  {
    if (m_buf[i] == '|')
    {
      MSG("found a pipe\n");
      // If bound, set to data mode
      if (is_bound())
      {
        set_is_data();
        return 0;
      }

      // If not bound, close.
      cout << socket() << ": rejecting attempt to convert unbound connection to data.\n";
      fail = -1;
      break;
    }
    else if (m_buf[i] == '\n' || m_buf[i] == '\r')
    {
      m_buf[i] = '\0';
      if (handle_management_command(m_buf + start))
      {
        // Pass the failure upward.
        fail = -1;
        return fail;
      }
      else
      {
        // point to the next command
        start = i + 1;
      }
    }
  }
  // Transfer any remaining unprocessed bytes to the start of the buffer.
  rem = m_buf_end - start;
  if (rem > 0)
    memmove(m_buf, m_buf + start, rem);
  m_buf_end = rem;

  // success
  return fail;
}

// Handle a single management connection command.
// cmd is a null-terminated string.
// return value: 0 on success, non-zero on failure.
int mmlink_connection::handle_management_command(char *cmd)
{
  int fail = 0;

  cout << "mmlink_connection::handle_management_command('" << cmd << "')\n";
  // Ignore empty string.
  if (!*cmd)
    return 0;

  if (!this->is_bound())
    fail = this->handle_unbound_command(cmd);
  else
    fail = this->handle_bound_command(cmd);

  return fail;
}

int mmlink_connection::handle_unbound_command(char *cmd)
{
  int fail = 0;
  //
  // Only HANDLE=xxxxxxxx is allowed
  // If wrong handle value, close
  // if any other input, close
  char expect_handle[] = "HANDLE 01234567";
  sprintf(expect_handle + strlen("HANDLE "), "%08X", get_server_id());
  if (0 == strcmp(expect_handle, cmd))
  {
    cout << socket() << ": accepted handle value (' "<< cmd << "'), setting to bound state\n";

    bind();
    send(OK, strlen(OK));
  }
  else
  {
     cout << socket() << ": closing socket: incorrect HANDLE value (expected: '"<< expect_handle << "'; got: '"<< cmd << "')\n";
    fail = -1;
  }

  return fail;
}

int mmlink_connection::handle_data()
{
  m_buf[m_buf_end] = '\0';
  cout << socket() << "(data): ";
  for (int i = 0; i < m_buf_end; ++i)
  {
    cout << setw(2) << m_buf[i] << " ";
  }
  cout << "\n";
  m_buf_end = 0;
}

int mmlink_connection::handle_bound_command(char *cmd)
{
  int arg1, arg2;
  bool unknown = true;

  if (1 == sscanf(cmd, "IDENT %X", &arg1))
  {
    if (arg1 >= 0 && arg1 <= 0xF) {
      int ident[4];
      size_t msg_len = 64;
      char msg[msg_len + 1];

      // Write the nibble value
      driver()->write_ident(arg1);
      driver()->ident(ident);
      snprintf(msg, msg_len, "%08X%08X%08X%08X\n",
        ident[3], ident[2], ident[1], ident[0],
        get_server_id());

      send(msg, strlen(msg));
      unknown = false;
    }
  }
  else if (1 == sscanf(cmd, "RESET %d", &arg1))
  {
    if (arg1 == 0 || arg1 == 1)
    {
      driver()->reset(arg1);
      send(OK, strlen(OK));
      unknown = false;
    }
  }
  else if (2 == sscanf(cmd, "ENABLE %d %d", &arg1, &arg2))
  {
    if (arg1 >= 0 && (arg2 == 0 || arg2 == 1))
    {
      driver()->enable(arg1, arg2);
      send(OK, strlen(OK));
      unknown = false;
    }
  }
  else if (0 == strncmp(cmd, "NOOP", 4))
  {
    send(OK, strlen(OK));
    unknown = false;
  }

  if (unknown)
    send(UNKNOWN, strlen(UNKNOWN));

  return 0;
}


