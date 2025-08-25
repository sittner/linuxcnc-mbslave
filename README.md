# LinuxCNC Modbus Slave HAL Driver

This is a HAL (Hardware Abstraction Layer) driver for LinuxCNC that implements a Modbus TCP slave, allowing external systems to read and write LinuxCNC HAL pins via Modbus protocol.

## Features

- **Modbus TCP Server**: Listens on configurable TCP ports
- **Multiple Data Types**: Supports various Modbus register types:
  - **Holding Registers** (read/write): 16-bit signed/unsigned, 32-bit signed/unsigned, float
  - **Input Registers** (read-only): 16-bit signed/unsigned, 32-bit signed/unsigned, float  
  - **Coils** (read/write): Digital outputs
  - **Discrete Inputs** (read-only): Digital inputs
- **Bit Mapping**: Individual bits within registers can be mapped to separate HAL pins
- **Byte/Word Swapping**: Configurable endianness handling
- **XML Configuration**: Easy setup through XML configuration files

## Prerequisites

- LinuxCNC installed and configured
- Development packages: `libexpat1-dev`, `linuxcnc-dev` (or equivalent)
- GCC compiler
- Make build system

## Installation

### From Source

1. **Clone or download the source code**
   ```bash
   git clone <repository-url>
   cd linuxcnc-mbslave
   ```

2. **Build the driver**
   ```bash
   make
   ```

3. **Install the driver**
   ```bash
   sudo make install
   ```
   This installs the `mbslave` binary to your LinuxCNC installation directory (typically `/usr/bin` or `~/linuxcnc-dev/bin`).

### Using Debian Package

If available, you can install using:
```bash
sudo dpkg -i linuxcnc-mbslave_*.deb
sudo apt-get install -f  # Install dependencies if needed
```

## Configuration

### XML Configuration File Location

The XML configuration file should be placed in one of these standard locations:

#### 1. **LinuxCNC Configuration Directory** (Recommended)
```
~/linuxcnc/configs/your-machine-name/mbslave-config.xml
```

Examples:
- `~/linuxcnc/configs/mill/mbslave-config.xml`
- `~/linuxcnc/configs/lathe/mbslave-config.xml`

#### 2. **System-wide Configuration Directory**
```
/etc/linuxcnc/mbslave-config.xml
```

#### 3. **Machine-specific Modbus Directory**
```
~/linuxcnc/configs/your-machine-name/modbus/mbslave-config.xml
```

#### 4. **Using the Example Configuration**
Copy the provided example:
```bash
cp examples/mbslave-conf.xml ~/linuxcnc/configs/your-machine-name/mbslave-config.xml
```

### XML Configuration Format

Create an XML configuration file (e.g., `mbslave-config.xml`) to define your Modbus slave setup:

```xml
<modbusSlaves>
  <modbusSlave name="mbslave">
    <tcpListener port="1502"/>
    
    <!-- Holding Registers (Read/Write) starting at address 3000 -->
    <holdingRegisters start="3000">
      <pin name="spindle-speed" type="s16"/>
      <pin name="feed-rate" type="u16"/>
      <pin name="position-x" type="float"/>
      <pin name="control-word" type="u32"/>
      
      <!-- Bit-mapped register -->
      <bitRegister>
        <pin name="enable-bit" bit="0"/>
        <pin name="ready-bit" bit="1"/>
        <pin name="alarm-bit" bit="2"/>
      </bitRegister>
    </holdingRegisters>
    
    <!-- Input Registers (Read-Only) starting at address 5000 -->
    <inputRegisters start="5000">
      <pin name="actual-position" type="float"/>
      <pin name="status-word" type="u16"/>
    </inputRegisters>
    
    <!-- Digital Inputs starting at address 1000 -->
    <inputs start="1000">
      <pin name="home-switch"/>
      <pin name="limit-switch"/>
    </inputs>
    
    <!-- Digital Outputs (Coils) starting at address 2000 -->
    <coils start="2000">
      <pin name="coolant-on"/>
      <pin name="spindle-enable"/>
    </coils>
  </modbusSlave>
</modbusSlaves>
```

### Supported Data Types

- **s16**: 16-bit signed integer (-32768 to 32767)
- **u16**: 16-bit unsigned integer (0 to 65535) 
- **s32**: 32-bit signed integer (uses 2 Modbus registers)
- **u32**: 32-bit unsigned integer (uses 2 Modbus registers)
- **float**: 32-bit floating point (uses 2 Modbus registers)

### Advanced Options

- **byteswap**: Swap bytes within each 16-bit word (`byteswap="true"`)
- **wordswap**: Swap word order for 32-bit values (`wordswap="true"`)

## Usage

### Starting the Driver

1. **Load in HAL configuration file** (`.hal` file):
   ```hal
   # Load the Modbus slave component
   loadusr -W mbslave ~/linuxcnc/configs/your-machine-name/mbslave-config.xml
   ```

2. **Connect HAL pins** in your HAL file:
   ```hal
   # Connect Modbus pins to your LinuxCNC HAL pins
   net spindle-speed-cmd mbslave.mbslave.spindle-speed => spindle.0.speed-in
   net coolant-enable mbslave.mbslave.coolant-on => iocontrol.0.coolant-flood
   ```

### Manual Testing

You can also run the driver manually for testing:
```bash
mbslave ~/linuxcnc/configs/your-machine-name/mbslave-config.xml
```

### HAL Pin Names

HAL pins are named using the pattern: `mbslave.<slave-name>.<pin-name>`

For the example configuration above:
- `mbslave.mbslave.spindle-speed`
- `mbslave.mbslave.feed-rate`
- `mbslave.mbslave.enable-bit`
- etc.

## Testing the Connection

You can test the Modbus connection using various tools:

### Using `mbpoll` (command line tool)
```bash
# Read holding registers starting at address 3000
mbpoll -a 1 -r 3000 -c 5 -t 4 127.0.0.1:1502

# Write to holding register
mbpoll -a 1 -r 3000 -t 4:int16 127.0.0.1:1502 1234
```

### Using Python with `pymodbus`
```python
from pymodbus.client.sync import ModbusTcpClient

client = ModbusTcpClient('127.0.0.1', port=1502)
result = client.read_holding_registers(3000, 5, unit=1)
print(result.registers)
client.close()
```

## Configuration Examples

### Simple Setup
Basic configuration for machine status monitoring:
```xml
<modbusSlaves>
  <modbusSlave name="status">
    <tcpListener port="1502"/>
    <inputRegisters start="0">
      <pin name="x-pos" type="float"/>
      <pin name="y-pos" type="float"/>
      <pin name="z-pos" type="float"/>
    </inputRegisters>
    <inputs start="0">
      <pin name="machine-on"/>
      <pin name="program-running"/>
    </inputs>
  </modbusSlave>
</modbusSlaves>
```

### Advanced Setup
Configuration with control capabilities:
```xml
<modbusSlaves>
  <modbusSlave name="control">
    <tcpListener port="1502"/>
    <holdingRegisters start="100">
      <pin name="jog-speed" type="float"/>
      <pin name="spindle-override" type="u16"/>
    </holdingRegisters>
    <coils start="0">
      <pin name="estop-reset"/>
      <pin name="cycle-start"/>
      <pin name="feed-hold"/>
    </coils>
  </modbusSlave>
</modbusSlaves>
```

## Troubleshooting

### Common Issues

1. **Port already in use**
   - Check if another process is using the configured port
   - Use `netstat -tln | grep 1502` to check

2. **HAL component fails to load**
   - Verify LinuxCNC is running and HAL is available
   - Check XML configuration syntax
   - Ensure all required dependencies are installed

3. **Cannot connect from Modbus client**
   - Verify firewall settings
   - Test with local connection first (`127.0.0.1`)
   - Check if the port is accessible: `telnet 127.0.0.1 1502`

### Debug Mode

Add debug output by running with verbose logging (if implemented) or check system logs:
```bash
dmesg | grep mbslave
journalctl -f
```

## Building from Source

The project uses a standard Make build system:

```bash
# Configure the build (detects LinuxCNC installation)
make configure

# Build all components
make all

# Clean build artifacts
make clean

# Install to system
make install
```

## Project Structure

- `src/` - Source code files
- `examples/` - Example configuration files
- `debian/` - Debian packaging files
- `Makefile` - Main build configuration
- `configure.mk` - Build system configuration

## License

This software is released under the terms specified in the LICENSE file.

## Development Status

- Current version: 0.9.0 (Initial release)
- See TODO file for planned improvements
- See ChangeLog for version history

## Support

- Review the ChangeLog for version history
- Check the TODO file for known limitations
- File issues or questions with the project maintainers
