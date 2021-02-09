use cobs::CobsDecoder;
use color_eyre::eyre::Result;
use object::read::{File as ElfFile, Object, ObjectSymbol};
use postform_decoder::{ElfMetadata, LogLevel, POSTFORM_VERSION};
use probe_rs::{
    config::registry,
    flashing::{download_file, Format},
    MemoryInterface, Probe, Session,
};
use probe_rs_rtt::{Rtt, ScanRegion};
use std::{
    fs,
    path::PathBuf,
    sync::{Arc, Mutex},
    time::Duration,
};
use structopt::StructOpt;
use termion::color;

fn print_probes() {
    let probes = Probe::list_all();

    if !probes.is_empty() {
        println!("The following devices were found:");
        probes
            .iter()
            .enumerate()
            .for_each(|(num, link)| println!("[{}]: {:?}", num, link));
    } else {
        println!("No devices were found.");
    }
}

fn print_chips() {
    let registry = registry::families().expect("Could not retrieve chip family registry");
    for chip_family in registry {
        println!("{}", chip_family.name);
        println!("    Variants:");
        for variant in chip_family.variants.iter() {
            println!("        {}", variant.name);
        }
    }
}

fn print_version() {
    // version from Cargo.toml e.g. "0.1.4"
    println!("{} {}", env!("CARGO_PKG_NAME"), env!("CARGO_PKG_VERSION"));
    println!("supported Postform version: {}", POSTFORM_VERSION);
}

#[derive(Debug, StructOpt)]
#[structopt()]
struct Opts {
    /// List supported chips and exit.
    #[structopt(long)]
    list_chips: bool,

    /// Lists all the connected probes and exit.
    #[structopt(long)]
    list_probes: bool,

    /// The chip.
    #[structopt(long, required_unless_one(&["list-chips", "list-probes", "version"]), env = "PROBE_RUN_CHIP")]
    chip: Option<String>,

    /// Path to an ELF firmware file.
    #[structopt(name = "ELF", parse(from_os_str), required_unless_one(&["list-chips", "list-probes", "version"]))]
    elf: Option<PathBuf>,

    #[structopt(long, short)]
    attach: bool,

    #[structopt(long, short = "V")]
    version: bool,
}

#[derive(Debug, thiserror::Error)]
enum RttError {
    #[error("Missing symbol {0}")]
    MissingSymbol(&'static str),
}

pub fn download_firmware(session: &Arc<Mutex<Session>>, elf_path: &PathBuf) -> Result<()> {
    let mut mutex_guard = session.lock().unwrap();
    println!("Loading FW to target");
    download_file(&mut mutex_guard, &elf_path, Format::Elf)?;

    let file_contents = fs::read(elf_path)?;
    let elf_file = ElfFile::parse(&file_contents[..])?;
    let main = elf_file
        .symbols()
        .find(|s| s.name().unwrap() == "main")
        .ok_or(RttError::MissingSymbol("main"))?;

    let mut core = mutex_guard.core(0).unwrap();
    let _ = core.reset_and_halt(Duration::from_millis(10))?;
    core.set_hw_breakpoint(main.address() as u32)?;
    core.run()?;
    core.wait_for_core_halted(Duration::from_secs(1))?;
    println!("Download complete!");

    Ok(())
}

#[derive(Debug)]
enum RttMode {
    NonBlocking = 1,
    Blocking = 2,
}

fn configure_rrt_mode(session: &Arc<Mutex<Session>>, rtt_addr: u64, mode: RttMode) -> Result<()> {
    let mut session_lock = session.lock().unwrap();
    let mut core = session_lock.core(0)?;
    let mode_flags_addr = rtt_addr as u32 + 44u32;
    println!("Setting mode to {:?}", mode);
    core.write_word_32(mode_flags_addr, mode as u32)?;

    Ok(())
}

pub fn run_core(session: Arc<Mutex<Session>>) -> Result<()> {
    let mut mutex_guard = session.lock().unwrap();
    let mut core = mutex_guard.core(0)?;
    core.clear_all_hw_breakpoints()?;
    core.run()?;
    Ok(())
}

fn color_for_level(level: LogLevel) -> String {
    match level {
        LogLevel::Debug => String::from(color::Green.fg_str()),
        LogLevel::Info => String::from(color::Yellow.fg_str()),
        LogLevel::Warning => color::Rgb(255u8, 0xA5u8, 0u8).fg_string(),
        LogLevel::Error => String::from(color::Red.fg_str()),
        LogLevel::Unknown => color::Rgb(255u8, 0u8, 0u8).fg_string(),
    }
}

fn handle_log(elf_metadata: &ElfMetadata, buffer: &[u8]) {
    match elf_metadata.parse(buffer) {
        Ok(log) => {
            println!(
                "{timestamp:<12.6} {color}{level:<11}{reset_color}: {msg}",
                timestamp = log.timestamp,
                color = color_for_level(log.level),
                level = log.level.to_string(),
                reset_color = color::Fg(color::Reset),
                msg = log.message
            );
            println!(
                "{color}└── File: {file_name}, Line number: {line_number}{reset}",
                color = color::Fg(color::LightBlack),
                file_name = log.file_name,
                line_number = log.line_number,
                reset = color::Fg(color::Reset)
            );
        }
        Err(error) => {
            println!(
                "{color}Error parsing log:{reset_color} {error}.",
                color = color::Fg(color::Red),
                error = error,
                reset_color = color::Fg(color::Reset)
            );
        }
    }
}

fn attach_rtt(session: Arc<Mutex<Session>>, elf_file: &ElfFile) -> Result<Rtt> {
    let segger_rtt = elf_file
        .symbols()
        .find(|s| s.name().unwrap() == "_SEGGER_RTT")
        .ok_or(RttError::MissingSymbol("_SEGGER_RTT"))?;
    println!("Attaching RTT to address 0x{:x}", segger_rtt.address());
    let scan_region = ScanRegion::Exact(segger_rtt.address() as u32);
    Ok(Rtt::attach_region(session, &scan_region)?)
}

fn main() -> Result<()> {
    color_eyre::install()?;

    let opts = Opts::from_args();

    if opts.list_probes {
        print_probes();
        return Ok(());
    }

    if opts.list_chips {
        print_chips();
        return Ok(());
    }

    if opts.version {
        print_version();
        return Ok(());
    }

    let elf_name = opts.elf.unwrap();
    let elf_metadata = ElfMetadata::from_elf_file(&elf_name)?;

    let probes = Probe::list_all();
    if probes.len() > 1 {
        println!("More than one probe conected! {:?}", probes);
        return Ok(());
    }
    let probe = probes[0].open()?;

    if let Some(chip) = opts.chip {
        let session = Arc::new(Mutex::new(probe.attach(chip)?));

        let elf_contents = fs::read(elf_name.clone())?;
        let elf_file = ElfFile::parse(&elf_contents)?;
        let segger_rtt = elf_file
            .symbols()
            .find(|s| s.name().unwrap() == "_SEGGER_RTT")
            .ok_or(RttError::MissingSymbol("_SEGGER_RTT"))?;
        let segger_rtt_addr = segger_rtt.address();

        {
            let session = session.clone();
            ctrlc::set_handler(move || {
                println!("Exiting application");
                configure_rrt_mode(&session, segger_rtt_addr, RttMode::NonBlocking)
                    .expect("Error setting NonBlocking mode");
                std::process::exit(0);
            })?;
        }
        if !opts.attach {
            download_firmware(&session, &elf_name)?;
        }
        configure_rrt_mode(&session, segger_rtt_addr, RttMode::Blocking)?;

        let mut rtt = attach_rtt(session.clone(), &elf_file)?;
        run_core(session)?;

        if let Some(log_channel) = rtt.up_channels().take(0) {
            let mut dec_buf = [0u8; 4096];
            let mut buf = [0u8; 4096];
            let mut decoder = CobsDecoder::new(&mut dec_buf);
            loop {
                let count = log_channel.read(&mut buf[..])?;
                for data_byte in buf.iter().take(count) {
                    match decoder.feed(*data_byte) {
                        Ok(Some(msg_len)) => {
                            drop(decoder);
                            handle_log(&elf_metadata, &dec_buf[..msg_len]);
                            decoder = CobsDecoder::new(&mut dec_buf[..]);
                        }
                        Err(decoded_len) => {
                            drop(decoder);
                            println!("Cobs decoding failed after {} bytes", decoded_len);
                            println!("Decoded buffer: {:?}", &dec_buf[..decoded_len]);
                            decoder = CobsDecoder::new(&mut dec_buf[..]);
                        }
                        Ok(None) => {}
                    }
                }
            }
        }
    }
    Ok(())
}
