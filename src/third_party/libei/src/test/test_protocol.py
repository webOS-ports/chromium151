#!/usr/bin/python3
#
# SPDX-License-Identifier: MIT
#
#
# EIS protocol test suite. This suite tests an EIS implementation, by default the
# eis-demo-server to see how whether it handles protocol messages correctly.
#
# To test another implementation:
# - set LIBEI_TEST_SOCKET to the path your EIS implementation is listening on
# - set LIBEI_TEST_SERVER to the executable of your EIS implementation,
#   or the empty string to connect to a running process
#
# To run $LIBEI_TEST_SERVER in valgrind, set LIBEI_USE_VALGRIND to a boolean true.
#
# e.g.
# $ export LIBEI_TEST_SOCKET=/run/user/1000/eis-0
# $ export LIBEI_TEST_SERVER=""
# $ pytest3 -v --log-level=DEBUG -k 'some string'
#
# Will run that test against whatever is providing that socket.

from functools import reduce
from typing import Generator, Optional
from pathlib import Path
from dataclasses import dataclass, field

import itertools
import os
import pytest
import subprocess
import time
import shlex
import signal
import socket
import structlog

try:
    from eiproto import (
        hexlify,
        Context,
        Interface,
        InterfaceName,
        MessageHeader,
        EiCallback,
        EiConnection,
        EiDevice,
        EiHandshake,
        EiSeat,
        EiTouchscreen,
    )
except ModuleNotFoundError as e:
    # This file needs to be processed by meson, so let's skip when this fails in the source dir
    if e.name == "eiproto":
        pytest.skip(allow_module_level=True)
    else:
        raise e


logger = structlog.get_logger()

VALGRIND_EXITCODE = 3


def VERSION_V(v):
    """Noop function that helps with grepping for hardcoded version numbers"""
    return v


@pytest.fixture
def socketpath(tmp_path) -> Path:
    test_socket_override = os.environ.get("LIBEI_TEST_SOCKET")
    if test_socket_override:
        return Path(test_socket_override)
    return Path(tmp_path) / "eis-0"


@pytest.fixture
def valgrind() -> list[str]:
    """
    Return the list of arguments to run our eis_executable in valgrind
    """
    if bool(os.environ.get("LIBEI_USE_VALGRIND", False)):
        valgrind = [
            "valgrind",
            "--leak-check=full",
            f"--error-exitcode={VALGRIND_EXITCODE}",
        ]
    else:
        valgrind = []

    return valgrind


@pytest.fixture
def eis_executable(valgrind, socketpath) -> Optional[list[str]]:
    """
    Returns a list of arguments of the EIS executable to run, to be passed
    into Popen.

    Returns None if we're expected to connect to an already running instance.
    """
    program = os.environ.get("LIBEI_TEST_SERVER", None)

    # if the variable is empty, use an existing running server
    if program == "":
        return None

    # If it's not set at all, we use our eis-demo-server
    if program is None:
        program = f"@LIBEI_TEST_SERVER@ --socketpath={socketpath} --verbose"  # set by meson to eis-demo-server

    return valgrind + shlex.split(program)


@pytest.fixture
def eis(socketpath, eis_executable) -> Generator["Eis", None, None]:
    if not eis_executable:
        yield Eis.create_existing_implementation(socketpath)
    else:
        eis = Eis.create(socketpath, eis_executable)
        yield eis
        eis.terminate()


@dataclass
class Ei:
    sock: socket.socket
    context: Context
    connection: Optional[EiConnection] = None
    interface_versions: dict[str, int] = field(init=False, default_factory=dict)
    seats: list[EiSeat] = field(init=False, default_factory=list)
    object_ids: Generator[int, None, None] = field(
        init=False, default_factory=lambda: itertools.count(3)
    )
    _data: bytes = field(init=False, default_factory=bytes)

    @property
    def data(self) -> bytes:
        return self._data

    def send(self, msg: bytes) -> None:
        logger.debug(f"sending {len(msg)} bytes", bytes=hexlify(msg))
        self.sock.sendmsg([msg])

    def find_objects_by_interface(self, interface: str) -> list[Interface]:
        return [o for o in self.context.objects.values() if o.name == interface]

    def callback_roundtrip(self) -> bool:
        assert self.connection is not None

        cb = EiCallback.create(next(self.object_ids), VERSION_V(1))
        self.context.register(cb)
        self.send(self.connection.Sync(cb.object_id, cb.version))

        return self.wait_for(
            lambda: cb not in self.find_objects_by_interface(InterfaceName.EI_CALLBACK)
        )

    @property
    def handshake(self) -> EiHandshake:
        setup = self.context.objects[0]
        assert isinstance(setup, EiHandshake)
        return setup

    def init_default_sender_connection(
        self, interface_versions: dict[str, int] = {}
    ) -> None:
        setup = self.handshake
        self.send(setup.HandshakeVersion(VERSION_V(1)))
        self.send(setup.ContextType(EiHandshake.EiContextType.SENDER))
        self.send(setup.Name("test client"))

        def on_interface_version(_, name, version):
            logger.debug(f"Interface {name} v{version}")
            assert version <= self.interface_versions[name]
            self.interface_versions[name] = version

        self.handshake.connect("InterfaceVersion", on_interface_version)

        for iname in filter(lambda i: i != InterfaceName.EI_HANDSHAKE, InterfaceName):
            version = interface_versions.get(iname, VERSION_V(1))
            self.interface_versions[iname] = version
            self.send(setup.InterfaceVersion(iname, version))

        self.send(setup.Finish())
        self.dispatch()

    def wait_for_seat(self, timeout=2) -> bool:
        def seat_is_done():
            return self.seats and [
                call for call in self.seats[0].calllog if call.name == "Done"
            ]

        return self.wait_for(seat_is_done, timeout)

    def wait_for_connection(self, timeout=2) -> bool:
        return self.wait_for(lambda: self.connection is not None, timeout)

    def wait_for(self, callable, timeout=2) -> bool:
        expire = time.time() + timeout
        while not callable():
            self.dispatch()
            if time.time() > expire:
                return False
            time.sleep(0.01)

        return True

    def seat_fill_capability_masks(self, seat: EiSeat):
        """
        Set up the seat to fill the interface masks for each Capability
        and add the bind_mask() helper function to compile a mask
        from interface names.
        """

        def seat_cap(seat, mask, intf_name):
            seat._interface_masks[intf_name] = mask

        seat._interface_masks = {}
        seat.connect("Capability", seat_cap)

        def bind_mask(interfaces: list[InterfaceName]) -> int:
            return reduce(
                lambda mask, v: mask | v,
                [seat._interface_masks[i] for i in interfaces],
                0,
            )

        seat.bind_mask = bind_mask

    def recv(self) -> bytes:
        try:
            data = self.sock.recv(1024)
            while data:
                self._data += data
                data = self.sock.recv(1024)
        except (BlockingIOError, ConnectionResetError):
            pass
        return self.data

    def dispatch(self, timeout=0.1) -> None:
        if not self.data:
            expire = time.time() + timeout
            while not self.recv():
                now = time.time()
                if now >= expire:
                    break
                time.sleep(min(0.01, expire - now))
                if now >= expire:
                    break

        while self.data:
            logger.debug("data pending dispatch: ", bytes=hexlify(self.data[:64]))
            header = MessageHeader.from_data(self.data)
            logger.debug("dispatching message: ", header=header)
            consumed = self.context.dispatch(self.data)
            if consumed == 0:
                break
            self.pop(consumed)

    def pop(self, count: int) -> None:
        self._data = self._data[count:]

    @classmethod
    def create(cls, socketpath: Path):
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM | socket.SOCK_NONBLOCK)
        while not socketpath.exists():
            time.sleep(0.01)

        for _ in range(3):
            try:
                sock.connect(os.fspath(socketpath))
                break
            except ConnectionRefusedError:
                time.sleep(0.1)
        else:
            assert False, "Failed to connect to EIS"

        ctx = Context.create()
        ei = cls(sock=sock, context=ctx)

        # callback for new objects
        def register_cb(interface: Interface) -> None:
            if isinstance(interface, EiConnection):
                assert ei.connection is None
                ei.connection = interface

                # Automatic ping/pong handler
                def ping(conn, id, version, new_objects={}):
                    pingpong = new_objects["ping"]
                    try:
                        ei.send(pingpong.Done(0))
                    except BrokenPipeError:
                        pass

                ei.connection.connect("Ping", ping)

            elif isinstance(interface, EiSeat):
                assert interface not in ei.seats

                seat = interface
                ei.seat_fill_capability_masks(seat)
                ei.seats.append(seat)

        def unregister_cb(interface: Interface) -> None:
            if interface == ei.connection:
                assert ei.connection is not None
                ei.connection = None
            elif interface in ei.seats:
                ei.seats.remove(interface)

        ctx.connect("register", register_cb)
        ctx.connect("unregister", unregister_cb)

        return ei


@dataclass
class Eis:
    process: Optional[subprocess.Popen]
    ei: Ei
    _stdout: Optional[str] = field(init=False, default=None)
    _stderr: Optional[str] = field(init=False, default=None)

    def terminate(self) -> None:
        if self.process is None:
            return

        def kill_gently(process) -> Generator[None, None, None]:
            process.send_signal(signal.SIGINT)
            yield
            process.terminate()
            yield
            process.kill()

        stdout, stderr = None, None
        for _ in kill_gently(self.process):
            try:
                stdout, stderr = self.process.communicate(timeout=1)
                break
            except subprocess.TimeoutExpired:
                pass

        if stdout:
            for line in stdout.split("\n"):
                logger.info(f"stdout: {line}")
        if stderr:
            for line in stderr.split("\n"):
                logger.info(f"stderr: {line}")
        self.process.wait()
        rc = self.process.returncode
        if rc not in [0, -signal.SIGTERM]:
            if rc == VALGRIND_EXITCODE:
                assert rc != VALGRIND_EXITCODE, (
                    "valgrind reported errors, see valgrind error messages"
                )
            else:
                assert rc == -signal.SIGTERM, (
                    f"Process exited with {signal.Signals(-rc).name}"
                )
        self.process = None  # allow this to be called multiple times

    @classmethod
    def create_existing_implementation(cls, socketpath) -> "Eis":
        ei = Ei.create(socketpath)
        return cls(process=None, ei=ei)

    @classmethod
    def create(cls, socketpath, executable) -> "Eis":
        process = subprocess.Popen(
            executable,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            encoding="utf-8",
            text=True,
            bufsize=1,
            universal_newlines=True,
        )
        ei = Ei.create(socketpath)
        return cls(process=process, ei=ei)


class TestEiProtocol:
    @property
    def using_demo_server(self) -> bool:
        return "@LIBEI_TEST_SERVER@".endswith("eis-demo-server")

    def test_server_sends_version_event_immediately(self, eis):
        """
        The server is expected to send ei_handshake.interface_version immediately
        on connect
        """
        ei = eis.ei
        ei.dispatch()

        setup = ei.context.objects[0]
        assert isinstance(setup, EiHandshake)

        ei.wait_for(lambda: bool(setup.calllog))

        call = setup.calllog[0]
        assert call.name == "HandshakeVersion"
        assert call.args["version"] == VERSION_V(1)

        eis.terminate()

    def test_server_sends_interface_version_events(self, eis):
        """
        The server is expected to send ei_handshake.interface_version immediately
        on connect
        """
        ei = eis.ei
        ei.dispatch()

        ei.init_default_sender_connection()
        ei.dispatch()
        ei.wait_for_connection()

        assert ei.interface_versions[InterfaceName.EI_CALLBACK] == VERSION_V(1)

        # Right now all our versions are 1, so let's ensure that's true
        for name, version in ei.interface_versions.items():
            print(name, version)
            assert version == VERSION_V(1), f"For interface {name}"

        eis.terminate()

    def test_server_sends_min_interface_version(self, eis):
        ei = eis.ei
        ei.dispatch()

        # Assign a random high number of the interfaces we claim to support
        interface_versions: dict[str, int] = {
            iface.name: idx + 3
            for idx, iface in enumerate(InterfaceName)
            if iface != InterfaceName.EI_HANDSHAKE
        }

        ei.init_default_sender_connection(interface_versions=interface_versions)
        ei.dispatch()
        ei.wait_for_connection()

        assert ei.interface_versions[InterfaceName.EI_CALLBACK] == VERSION_V(1)

        # Right now all our EIS versions are 1, despite whatever we announce
        for name, version in ei.interface_versions.items():
            assert version == VERSION_V(1), f"For interface {name}"

        eis.terminate()

    def test_send_wrong_context_type(self, eis):
        """
        Connect with an invalid context type, expect to be disconnected
        """
        ei = eis.ei
        ei.dispatch()

        # Pick some random type (and make sure it's not a valid type in the current API)
        invalid_type = 4
        try:
            EiHandshake.EiContextType(invalid_type)
            assert False, (
                f"{invalid_type} should not be a valid ContextType, this test needs an update"
            )
        except ValueError:
            pass

        ei.send(ei.handshake.HandshakeVersion(VERSION_V(1)))
        ei.send(ei.handshake.ContextType(invalid_type))

        try:
            # The server either disconnects the socket because we sent garbage
            # or immediately disconnects us after the .done request
            ei.dispatch()

            for interface in [
                InterfaceName.EI_CONNECTION,
                InterfaceName.EI_CALLBACK,
                InterfaceName.EI_PINGPONG,
            ]:
                ei.send(
                    ei.handshake.InterfaceVersion(interface, VERSION_V(1))
                )  # these are required
            ei.send(ei.handshake.Finish())
            ei.dispatch()

            ei.wait_for_connection(timeout=1)

            # ok, still not socket-disconnected, let's make sure
            # we did immediately get a Disconnected message
            if ei.connection:
                assert ei.connection is not None
                call = ei.connection.calllog[0]
                assert call.name == "Disconnected"
                assert call.args["reason"] == EiConnection.EiDisconnectReason.ERROR
                assert call.args["explanation"] is not None

            # Now let's trigger a BrokenPipeError
            ei.send(bytes(16))

            assert False, "The server should have disconnected us"

        except (ConnectionResetError, BrokenPipeError):
            pass

        eis.terminate()

    def test_connect_and_disconnect(self, eis):
        """
        Connect to the server with a valid sequence, then disconnect
        once we get the connection object
        """
        ei = eis.ei

        # drain any messages
        ei.dispatch()

        # Establish our connection
        ei.init_default_sender_connection()
        ei.wait_for_connection()

        # This should've set our connection object
        assert ei.connection is not None
        connection = ei.connection
        ei.send(connection.Disconnect())
        try:
            # Send disconnect twice, just to test that case, should be ignored by the
            # server
            ei.send(connection.Disconnect())

            ei.dispatch()
            time.sleep(0.1)
            ei.dispatch()
        except (ConnectionResetError, BrokenPipeError):
            pass

        ei.wait_for(lambda: any(c.name == "Disconnect" for c in connection.calllog))

        for call in connection.calllog:
            assert call.name != "Disconnected", "No disconnect event allowed here"

        try:
            ei.send(connection.Disconnect())
            assert False, "Expected socket to be closed"
        except BrokenPipeError:
            pass

        eis.terminate()

    @pytest.mark.skipif(
        not getattr(int, "bit_count", None), reason="int.bit_count() required"
    )
    def test_connect_receive_seat(self, eis):
        """
        Ensure we get a seat object after setting our connection
        """
        ei = eis.ei
        ei.dispatch()
        setup = ei.handshake

        # Establish our connection
        ei.send(setup.HandshakeVersion(VERSION_V(1)))
        ei.send(setup.ContextType(EiHandshake.EiContextType.SENDER))
        ei.send(setup.Name("test client"))
        for interface in [
            InterfaceName.EI_CONNECTION,
            InterfaceName.EI_CALLBACK,
            InterfaceName.EI_PINGPONG,
            InterfaceName.EI_POINTER,
            InterfaceName.EI_POINTER_ABSOLUTE,
            InterfaceName.EI_KEYBOARD,
            InterfaceName.EI_TOUCHSCREEN,
        ]:
            ei.send(
                setup.InterfaceVersion(interface, VERSION_V(1))
            )  # these are required
        ei.send(
            setup.InterfaceVersion(InterfaceName.EI_SEAT, VERSION_V(100))
        )  # excessive version
        ei.send(
            setup.InterfaceVersion(InterfaceName.EI_DEVICE, VERSION_V(100))
        )  # excessive version
        ei.send(setup.Finish())
        ei.dispatch()

        ei.wait_for_seat()

        assert ei.seats
        for seat in ei.seats:
            assert seat.version == 1  # we have 100, but the server only has 1
            for call in seat.calllog:
                if call.name == "Capability":
                    assert call.args["mask"].bit_count() == 1
                    assert InterfaceName(call.args["interface"])

            if self.using_demo_server:
                all_caps = [
                    call.args["interface"]
                    for call in seat.calllog
                    if call.name == "Capability"
                ]
                assert sorted(all_caps) == sorted(
                    [
                        i.value
                        for i in (
                            InterfaceName.EI_POINTER,
                            InterfaceName.EI_POINTER_ABSOLUTE,
                            InterfaceName.EI_BUTTON,
                            InterfaceName.EI_SCROLL,
                            InterfaceName.EI_KEYBOARD,
                            InterfaceName.EI_TOUCHSCREEN,
                        )
                    ]
                )

            for call in seat.calllog:
                if call.name == "Name":
                    assert call.args["name"] is not None
                    if self.using_demo_server:
                        assert call.args["name"] == "default"
                    break
            else:
                assert False, f"Expected ei_seat.name, but got none in {seat.calllog}"

            for call in seat.calllog:
                if call.name == "Done":
                    break
            else:
                assert False, f"Expected ei_seat.done, but got none in {seat.calllog}"

    def test_connect_no_seat_without_ei_seat(self, eis):
        """
        Ensure we do not get a seat object if we don't announce support for ei_seat
        """
        ei = eis.ei
        ei.dispatch()
        setup = ei.handshake

        # Establish our connection
        ei.send(setup.HandshakeVersion(VERSION_V(1)))
        ei.send(setup.ContextType(EiHandshake.EiContextType.SENDER))
        ei.send(setup.Name("test client"))
        for interface in [
            InterfaceName.EI_CONNECTION,
            InterfaceName.EI_CALLBACK,
            InterfaceName.EI_PINGPONG,
        ]:
            ei.send(
                setup.InterfaceVersion(interface, VERSION_V(1))
            )  # these are required
        # Do not announce ei_seat support
        ei.send(setup.Finish())
        ei.dispatch()

        assert not ei.seats
        eis.terminate()

    def test_seat_bind_no_caps(self, eis):
        """
        Ensure nothing happens if we bind to a seat with capabilities outside what is supported
        """
        ei = eis.ei
        ei.dispatch()
        ei.init_default_sender_connection()
        ei.wait_for_seat()

        seat = ei.seats[0]
        ei.send(seat.Bind(0x00))  # binding to no caps is fine
        ei.dispatch()
        time.sleep(0.1)
        ei.dispatch()

        eis.terminate()

    def test_seat_bind_invalid_caps_expect_disconnection(self, eis):
        ei = eis.ei
        ei.dispatch()
        ei.init_default_sender_connection()
        ei.wait_for_seat()

        connection = ei.connection
        assert connection is not None
        seat = ei.seats[0]
        ei.send(seat.Bind(0x1))  # binding to invalid caps should get us disconnected
        try:
            ei.dispatch()
            time.sleep(0.1)
            ei.dispatch()

            for call in seat.calllog:
                if call.name == "Destroyed":
                    break
            else:
                assert False, "Expected seat to get destroyed but didn't"

            for call in connection.calllog:
                if call.name == "Disconnected":
                    assert call.args["reason"] == EiConnection.EiDisconnectReason.VALUE
                    assert "Invalid capabilities" in call.args["explanation"]
                    break
            else:
                assert False, "Expected disconnection event"
        except ConnectionResetError:
            pass

        eis.terminate()

    @pytest.mark.parametrize("bind_first", (True, False))
    def test_seat_release_expect_destroyed(self, eis, bind_first):
        ei = eis.ei
        ei.dispatch()
        ei.init_default_sender_connection()
        ei.wait_for_seat()

        seat = ei.seats[0]

        have_seat_destroyed = False

        def destroyed_cb(_, serial):
            nonlocal have_seat_destroyed
            have_seat_destroyed = True

        seat.connect("Destroyed", destroyed_cb)
        if bind_first:
            ei.send(
                seat.Bind(
                    seat.bind_mask(
                        [
                            InterfaceName.EI_POINTER,
                            InterfaceName.EI_BUTTON,
                            InterfaceName.EI_SCROLL,
                        ]
                    )
                )
            )
        ei.send(seat.Release())

        ei.dispatch()
        ei.wait_for(lambda: have_seat_destroyed)

    def test_connection_sync(self, eis):
        """
        Test the ei_connection.sync() callback mechanism
        """

        ei = eis.ei
        ei.dispatch()
        ei.init_default_sender_connection()
        ei.wait_for_seat()

        cb = EiCallback.create(next(ei.object_ids), VERSION_V(1))
        ei.context.register(cb)
        assert ei.connection is not None
        ei.send(ei.connection.Sync(cb.object_id, cb.version))
        ei.dispatch()

        assert cb.calllog[0].name == "Done"
        assert cb.calllog[0].args["callback_data"] == 0  # hardcoded in libeis for now

    def test_invalid_object(self, eis):
        """
        Send a message for an invalid object and ensure we get the event back
        """

        ei = eis.ei
        ei.dispatch()
        ei.init_default_sender_connection()
        ei.wait_for_seat()

        seat: EiSeat = ei.seats[0]

        have_invalid_object_event = False
        have_sync = False

        def invalid_object_cb(_, last_serial, id):
            nonlocal have_invalid_object_event
            assert id == seat.object_id
            have_invalid_object_event = True

        ei.connection.connect("InvalidObject", invalid_object_cb)

        release = seat.Release()

        ei.send(release)
        ei.dispatch()
        cb = EiCallback.create(next(ei.object_ids), VERSION_V(1))
        ei.context.register(cb)

        def sync_cb(_, unused):
            nonlocal have_sync
            have_sync = True

        cb.connect("Done", sync_cb)

        # Send the invalid object request
        ei.send(release)

        ei.send(ei.connection.Sync(cb.object_id, cb.version))
        ei.wait_for(lambda: have_sync)

        assert have_invalid_object_event, "Expected invalid_object event, got none"

    def test_disconnect_before_setup_finish(self, eis):
        ei = eis.ei
        ei.dispatch()
        ei.send(ei.handshake.ContextType(EiHandshake.EiContextType.SENDER))
        ei.sock.close()
        time.sleep(0.5)
        # Not much we can test here other than hoping the EIS implementation doesn't segfault

    @pytest.mark.parametrize(
        "missing_interface",
        (
            InterfaceName.EI_CALLBACK,
            InterfaceName.EI_CONNECTION,
            InterfaceName.EI_PINGPONG,
            InterfaceName.EI_SEAT,
            InterfaceName.EI_DEVICE,
            InterfaceName.EI_POINTER,
        ),
    )
    def test_connect_without_ei_interfaces(self, eis, missing_interface):
        ei = eis.ei
        ei.dispatch()

        setup = ei.handshake
        ei.send(setup.HandshakeVersion(VERSION_V(1)))
        ei.send(setup.ContextType(EiHandshake.EiContextType.SENDER))
        ei.send(setup.Name("test client"))

        for iname in filter(lambda i: i != InterfaceName.EI_HANDSHAKE, InterfaceName):
            if iname != missing_interface:
                ei.send(setup.InterfaceVersion(iname, VERSION_V(1)))

        @dataclass
        class Status:
            connected: bool = False
            disconnected: bool = False
            seats: bool = False
            devices: bool = False

        status = Status()

        try:

            def on_device(seat, id, version, new_objects={}):
                assert missing_interface not in [InterfaceName.EI_DEVICE]
                status.devices = True

            def on_seat(connection, id, version, new_objects={}):
                assert missing_interface not in [InterfaceName.EI_SEAT]
                seat = new_objects["seat"]
                assert seat is not None
                seat.connect("Device", on_device)

                def on_done(seat):
                    if missing_interface != InterfaceName.EI_POINTER:
                        mask = seat.bind_mask([InterfaceName.EI_POINTER])
                    else:
                        # Need to bind to *something* to get at least one device
                        mask = seat.bind_mask([InterfaceName.EI_KEYBOARD])
                        ei.send(seat.Bind(mask))

                seat.connect("Done", on_done)
                status.seats = True

            def on_disconnected(connection, last_serial, reason, explanation):
                assert missing_interface in [
                    InterfaceName.EI_SEAT,
                    InterfaceName.EI_DEVICE,
                ]
                status.disconnected = True

            def on_connection(setup, serial, id, version, new_objects={}):
                # these three must be present, otherwise we get disconnected
                assert missing_interface not in [
                    InterfaceName.EI_CONNECTION,
                    InterfaceName.EI_CALLBACK,
                    InterfaceName.EI_PINGPONG,
                ]
                status.connected = True
                connection = new_objects["connection"]
                assert connection is not None
                connection.connect("Seat", on_seat)
                connection.connect("Disconnected", on_disconnected)

            setup.connect("Connection", on_connection)

            ei.send(setup.Finish())
            ei.dispatch()

            if missing_interface in [
                InterfaceName.EI_CONNECTION,
                InterfaceName.EI_CALLBACK,
                InterfaceName.EI_PINGPONG,
            ]:
                # valgrind is slow, so let's wait for it to catch up
                time.sleep(0.3)
                ei.dispatch()
                assert not status.connected
                assert not status.disconnected  # we never get the Disconnected event

                # Might take a while but eventually we should get disconnected...
                for _ in range(10):
                    if ei.connection is None:
                        return
                    time.sleep(0.1)
                    ei.dispatch()
                else:
                    assert False, "We should've been disconnected by now"

            ei.wait_for(lambda: status.connected)
            if missing_interface in [InterfaceName.EI_DEVICE, InterfaceName.EI_SEAT]:
                assert ei.wait_for(lambda: status.disconnected)
                assert status.disconnected, (
                    f"Expected to be disconnected for missing {missing_interface}"
                )
            else:
                assert (
                    missing_interface == InterfaceName.EI_POINTER
                )  # otherwise we shouldn't get here
                assert ei.callback_roundtrip(), "Callback roundtrip failed"
                assert status.connected
                assert not status.disconnected
                assert ei.wait_for(lambda: status.seats)
                assert ei.wait_for(lambda: status.devices)
                assert status.devices

        except BrokenPipeError:
            assert missing_interface in [
                InterfaceName.EI_CONNECTION,
                InterfaceName.EI_CALLBACK,
                InterfaceName.EI_PINGPONG,
            ]

    @pytest.mark.parametrize("test_for", ("repeat-id", "invalid-id", "decreasing-id"))
    def test_invalid_object_id(self, eis, test_for):
        """
        Expect to get disconnected if we allocate a client ID in the server ID range
        or if we allocate the same client id twice
        """
        ei = eis.ei
        ei.dispatch()

        ei.init_default_sender_connection()
        ei.dispatch()
        ei.wait_for_connection()

        @dataclass
        class Status:
            disconnected: bool = False
            reason: int = 0
            explanation: Optional[str] = None

        status = Status()

        def on_disconnected(connection, last_serial, reason, explanation):
            status.disconnected = True
            status.reason = reason
            status.explanation = explanation

        ei.connection.connect("Disconnected", on_disconnected)

        if test_for == "invalid-id":
            # random id in the server range, 0xff..00 is used by the connection
            # and some of the next few ids might have been used by pingpongs
            cb = EiCallback.create(0xFF00000000000100, VERSION_V(1))
            ei.context.register(cb)
            ei.send(ei.connection.Sync(cb.object_id, cb.version))
        elif test_for == "repeat-id":
            cb = EiCallback.create(0x100, VERSION_V(1))
            ei.context.register(cb)
            ei.send(ei.connection.Sync(cb.object_id, cb.version))
            cb = EiCallback.create(0x100, VERSION_V(1))
            ei.send(ei.connection.Sync(cb.object_id, cb.version))
        elif test_for == "decreasing-id":
            cb = EiCallback.create(0x101, VERSION_V(1))
            ei.context.register(cb)
            ei.send(ei.connection.Sync(cb.object_id, cb.version))
            cb = EiCallback.create(0x100, VERSION_V(1))
            ei.context.register(cb)
            ei.send(ei.connection.Sync(cb.object_id, cb.version))
        else:
            assert False, "Unhandled test parameter"

        ei.wait_for(lambda: status.disconnected)
        assert status.disconnected
        assert status.reason == EiConnection.EiDisconnectReason.PROTOCOL, (
            status.explanation
        )
        assert status.explanation is not None

    def test_invalid_callback_version(self, eis):
        """
        Expect to get disconnected if we allocate a client object outside the agreed version range.
        Right now only callbacks are client-created, so that's all we can test here.
        """
        ei = eis.ei
        ei.dispatch()

        ei.init_default_sender_connection()
        ei.dispatch()
        ei.wait_for_connection()

        @dataclass
        class Status:
            disconnected: bool = False
            reason: int = 0
            explanation: Optional[str] = None

        status = Status()

        def on_disconnected(connection, last_serial, reason, explanation):
            status.disconnected = True
            status.reason = reason
            status.explanation = explanation

        ei.connection.connect("Disconnected", on_disconnected)

        cb = EiCallback.create(0x100, VERSION_V(100))
        ei.context.register(cb)
        ei.send(ei.connection.Sync(cb.object_id, cb.version))

        ei.wait_for(lambda: status.disconnected)
        assert status.disconnected
        assert status.reason == EiConnection.EiDisconnectReason.PROTOCOL, (
            status.explanation
        )
        assert status.explanation is not None

    @pytest.mark.parametrize(
        "wanted_interface",
        (
            InterfaceName.EI_POINTER,
            InterfaceName.EI_KEYBOARD,
            InterfaceName.EI_TOUCHSCREEN,
        ),
    )
    def test_connect_receive_device(self, eis, wanted_interface):
        """
        Ensure we get a device object after binding to a seat
        """
        ei = eis.ei

        @dataclass
        class Status:
            capability: Optional[Interface] = None  # type: ignore

        status = Status()

        def on_interface(device, object, name, version, new_objects):
            logger.debug(
                "new capability",
                device=device,
                object=object,
                name=name,
                version=version,
            )
            if name == wanted_interface:
                status.capability = new_objects["object"]

        def on_new_device(seat, device, version, new_objects):
            logger.debug("new device", object=new_objects["device"])
            new_objects["device"].connect("Interface", on_interface)

        def on_new_object(o: Interface):
            logger.debug("new object", object=o)
            if o.name == InterfaceName.EI_SEAT:
                ei.seat_fill_capability_masks(o)
                o.connect("Device", on_new_device)

        ei.context.connect("register", on_new_object)
        ei.dispatch()
        ei.init_default_sender_connection()

        ei.wait_for_seat()
        seat = ei.seats[0]
        ei.send(
            seat.Bind(
                seat.bind_mask(
                    [
                        InterfaceName.EI_POINTER,
                        InterfaceName.EI_POINTER_ABSOLUTE,
                        InterfaceName.EI_KEYBOARD,
                        InterfaceName.EI_TOUCHSCREEN,
                    ]
                )
            )
        )

        ei.wait_for(lambda: status.capability is not None)

        assert status.capability is not None
        assert status.capability.name == wanted_interface
        assert status.capability.version == VERSION_V(1)

    @pytest.mark.parametrize(
        "wanted_pointer",
        (InterfaceName.EI_POINTER, InterfaceName.EI_POINTER_ABSOLUTE),
    )
    def test_connect_receive_pointer(self, eis, wanted_pointer):
        """
        Ensure we get the correct pointer device after binding
        """
        ei = eis.ei

        @dataclass
        class Status:
            pointers: dict[InterfaceName, Interface] = field(default_factory=dict)
            all_caps: int = 0

        status = Status()

        def on_interface(device, object, name, version, new_objects):
            logger.debug(
                "new capability",
                device=device,
                object=object,
                name=name,
                version=version,
            )
            if name in [InterfaceName.EI_POINTER, InterfaceName.EI_POINTER_ABSOLUTE]:
                status.pointers[InterfaceName(name)] = new_objects["object"]

        def on_new_device(seat, device, version, new_objects):
            logger.debug("new device", object=new_objects["device"])
            new_objects["device"].connect("Interface", on_interface)

        def on_new_object(o: Interface):
            logger.debug("new object", object=o)
            if o.name == InterfaceName.EI_SEAT:
                ei.seat_fill_capability_masks(o)
                o.connect("Device", on_new_device)

        ei.context.connect("register", on_new_object)
        ei.dispatch()
        ei.init_default_sender_connection()

        ei.wait_for_seat()
        seat = ei.seats[0]
        ei.send(seat.Bind(seat.bind_mask([wanted_pointer])))

        ei.wait_for(lambda: status.pointers)

        assert status.pointers[wanted_pointer] is not None
        assert len(status.pointers) == 1

    @pytest.mark.parametrize("ei_touchscreen_version", (1, 2))
    def test_touch_cancel_check_version(self, eis, ei_touchscreen_version):
        """
        Ensure EIS disconnects us (or not) if we send a touch cancel event,
        depending whether it's supported.
        """

        ei = eis.ei

        @dataclass
        class Status:
            device: EiDevice = None
            touchscreen: Optional[EiTouchscreen] = None
            disconnected: bool = False
            resumed: bool = False
            serial: int = 0

        status = Status()

        def on_interface(device, object, name, version, new_objects):
            logger.debug(
                "new capability",
                device=device,
                object=object,
                name=name,
                version=version,
            )
            if name == InterfaceName.EI_TOUCHSCREEN:
                assert status.touchscreen is None
                status.touchscreen = new_objects["object"]

        def on_device_resumed(device, serial):
            status.resumed = True
            status.serial = serial

        def on_new_device(seat, device, version, new_objects):
            logger.debug("new device", object=new_objects["device"])
            status.device = new_objects["device"]
            status.device.connect("Interface", on_interface)
            status.device.connect("Resumed", on_device_resumed)

        def on_new_object(o: Interface):
            logger.debug("new object", object=o)
            if o.name == InterfaceName.EI_SEAT:
                ei.seat_fill_capability_masks(o)
                o.connect("Device", on_new_device)

        ei.context.connect("register", on_new_object)
        ei.dispatch()

        def on_disconnected(connection, last_serial, reason, explanation):
            status.disconnected = True

        def on_connection(setup, serial, id, version, new_objects={}):
            connection = new_objects["connection"]
            connection.connect("Disconnected", on_disconnected)

        setup = ei.handshake
        setup.connect("Connection", on_connection)
        ei.init_default_sender_connection(
            interface_versions={"ei_touchscreen": ei_touchscreen_version}
        )

        assert ei.interface_versions[InterfaceName.EI_TOUCHSCREEN] == VERSION_V(
            ei_touchscreen_version
        )

        ei.wait_for_seat()
        seat = ei.seats[0]
        ei.send(seat.Bind(seat.bind_mask([InterfaceName.EI_TOUCHSCREEN])))
        ei.wait_for(lambda: status.touchscreen and status.resumed)

        assert status.touchscreen is not None

        ei.send(status.device.StartEmulating(status.serial, 123))
        logger.debug("Sending touch events")
        touchid = 1
        touchscreen = status.touchscreen
        device = status.device
        ei.send(touchscreen.Down(touchid, 10, 20))
        ei.send(device.Frame(status.serial, int(time.time())))
        ei.send(touchscreen.Motion(touchid, 10, 25))
        ei.send(device.Frame(status.serial, int(time.time())))
        ei.send(touchscreen.Cancel(touchid))
        try:
            ei.send(device.Frame(status.serial, int(time.time())))
        except BrokenPipeError:
            pass

        ei.dispatch()
        ei.wait_for(lambda: status.disconnected)

        if ei_touchscreen_version == 1:
            assert status.disconnected is True
        else:
            ei.callback_roundtrip()
            assert status.disconnected is False
