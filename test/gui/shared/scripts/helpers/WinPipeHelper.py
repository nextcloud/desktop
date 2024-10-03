import os
import time

# pylint: disable=import-error
import win32pipe
import win32file
import winerror
import pywintypes
import win32event

TIMEOUT = 100
DEFAULT_BUFLEN = 4096
CLIENT_MESSAGES = [
    'STATUS',
    'REGISTER_PATH',
    'UNREGISTER_PATH',
    'UPDATE_VIEW',
    'GET_STRINGS',
    'STRING',
    'VERSION',
    'SHARE',
    'GET_MENU_ITEMS',
    'MENU_ITEM',
]


def get_pipe_path():
    pipename = r'\\.\\pipe\\'
    pipename = os.path.join(pipename, 'ownCloud-' + os.getenv('USERNAME'))
    return pipename


class WinPipeConnect:
    def __init__(self):
        self.connected = False
        self._pipe = None
        self._remainder = ''.encode('utf-8')
        self._overlapped = pywintypes.OVERLAPPED()
        self._overlapped.hEvent = win32event.CreateEvent(None, 1, 0, None)
        self.connect_to_pipe_server()

    def connect_to_pipe_server(self):
        try:
            pipename = get_pipe_path()

            self._pipe = win32file.CreateFile(
                pipename,
                win32file.GENERIC_READ | win32file.GENERIC_WRITE,
                0,
                None,
                win32file.OPEN_EXISTING,
                win32file.FILE_FLAG_OVERLAPPED,
                None,
            )

            if self._pipe == win32file.INVALID_HANDLE_VALUE:
                win32pipe.CloseHandle(self._pipe)
                raise OSError('Invalid _pipe value')

            self.connected = True

            self.sendCommand('VERSION:\n')
            self.sendCommand('GET_STRINGS:\n')
        except Exception as e:  # pylint: disable=broad-except
            print(f'Could not connect to named pipe {pipename}\n' + str(e))
            win32file.CloseHandle(self._pipe)

    def sendCommand(self, cmd):  # pylint: disable=invalid-name
        if self.connected:
            w_res, _ = win32file.WriteFile(
                self._pipe, cmd.encode('utf-8'), self._overlapped
            )
            if w_res == winerror.ERROR_IO_PENDING:
                res = win32event.WaitForSingleObject(self._overlapped.hEvent, TIMEOUT)
                if res != win32event.WAIT_OBJECT_0:
                    print('Sending timed out!')
                    return False
                if not win32file.GetOverlappedResult(
                    self._pipe, self._overlapped, False
                ):
                    print('GetOverlappedResult failed')
                    return False
        else:
            print('Cannot send, not connected!')
            return False
        return True

    # Reads data that becomes available.
    # New responses can be accessed with get_available_responses().
    def read_socket_data_with_timeout(self, timeout):
        messages = b''
        start_time = time.time()
        while True:  # pylint: disable=too-many-nested-blocks
            if (time.time() - start_time) >= timeout:
                self._remainder += messages
                break

            peek_bytes = win32pipe.PeekNamedPipe(self._pipe, DEFAULT_BUFLEN)[1]
            if isinstance(peek_bytes, int) and peek_bytes > 0:
                _, message_mem = win32file.ReadFile(
                    self._pipe, DEFAULT_BUFLEN, self._overlapped
                )
                if message_mem:
                    m_bytes = bytes(message_mem)
                    if b'\n' in m_bytes:
                        for m in m_bytes.split(b'\n'):
                            try:
                                # append decodable and client specific bytes
                                m.decode('utf-8')
                                start = m.split(b':', 1)[0]
                                if start.decode('utf-8') in CLIENT_MESSAGES:
                                    messages += m + b'\n'
                            except:
                                pass

            else:
                res = win32event.WaitForSingleObject(
                    self._overlapped.hEvent, int(timeout * 1000)
                )
                if res != win32event.WAIT_OBJECT_0:
                    print('Reading timed out!')
                    return False
                if not win32file.GetOverlappedResult(
                    self._pipe, self._overlapped, False
                ):
                    return False
        return True

    # Parses response lines out of collected data, returns list of strings
    def get_available_responses(self):
        if (end := self._remainder.rfind(b'\n')) == -1:
            return []
        data = self._remainder[:end]
        self._remainder = self._remainder[end + 1 :]
        return data.decode('utf-8').split('\n')

    def close_conn(self):
        win32file.CloseHandle(self._pipe)
