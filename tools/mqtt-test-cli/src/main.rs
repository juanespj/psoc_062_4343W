use std::time::Duration;

use anyhow::{Context, Result};
use clap::{Parser, Subcommand};
use rumqttc::{Client, Event, Incoming, MqttOptions, QoS};

#[derive(Debug, Parser)]
#[command(name = "mqtt-test-cli", version, about = "MQTT helper to test psoc debug messages")]
struct Cli {
    /// MQTT broker host
    #[arg(long, default_value = "test.mosquitto.org")]
    host: String,

    /// MQTT broker port
    #[arg(long, default_value_t = 1883)]
    port: u16,

    /// MQTT client ID
    #[arg(long, default_value = "psoc6-rust-tester")]
    client_id: String,

    #[command(subcommand)]
    cmd: Command,
}

#[derive(Debug, Subcommand)]
enum Command {
    /// Subscribe to topic and print messages
    Sub {
        #[arg(long, default_value = "psoc6/debug")]
        topic: String,
    },
    /// Publish one message to topic
    Pub {
        #[arg(long, default_value = "psoc6/debug")]
        topic: String,
        #[arg(long)]
        message: String,
    },
}

fn make_opts(host: String, port: u16, client_id: String) -> MqttOptions {
    let mut opts = MqttOptions::new(client_id, host, port);
    opts.set_keep_alive(Duration::from_secs(20));
    opts
}

fn run_sub(opts: MqttOptions, topic: String) -> Result<()> {
    let (client, mut connection) = Client::new(opts, 10);
    client
        .subscribe(topic.clone(), QoS::AtMostOnce)
        .with_context(|| format!("failed to subscribe to {topic}"))?;

    println!("Subscribed to {topic}. Waiting for messages...");
    for notification in connection.iter() {
        match notification {
            Ok(Event::Incoming(Incoming::Publish(p))) => {
                let payload = String::from_utf8_lossy(&p.payload);
                println!("[{}] {}", p.topic, payload);
            }
            Ok(_) => {}
            Err(e) => {
                eprintln!("connection error: {e}");
                std::thread::sleep(Duration::from_millis(500));
            }
        }
    }
    Ok(())
}

fn run_pub(opts: MqttOptions, topic: String, message: String) -> Result<()> {
    let (client, mut connection) = Client::new(opts, 10);

    std::thread::spawn(move || {
        for event in connection.iter() {
            if let Err(e) = event {
                eprintln!("connection error: {e}");
                break;
            }
        }
    });

    client
        .publish(topic.clone(), QoS::AtMostOnce, false, message.as_bytes())
        .with_context(|| format!("failed to publish to {topic}"))?;
    println!("Published to {topic}");
    std::thread::sleep(Duration::from_millis(300));
    Ok(())
}

fn main() -> Result<()> {
    let cli = Cli::parse();
    let opts = make_opts(cli.host, cli.port, cli.client_id);

    match cli.cmd {
        Command::Sub { topic } => run_sub(opts, topic),
        Command::Pub { topic, message } => run_pub(opts, topic, message),
    }
}
