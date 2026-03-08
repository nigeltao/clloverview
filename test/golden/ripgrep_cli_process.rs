// https://github.com/BurntSushi/ripgrep/blob/master/crates/cli/src/process.rs
// with further reductions. We just need syntax examples, not full code.

impl CommandReaderBuilder {
    pub fn new() -> CommandReaderBuilder {
        CommandReaderBuilder::default()
    }

    pub fn build(
        &self,
        command: &mut process::Command,
    ) -> Result<CommandReader, CommandError> {
        let mut child = command
            .stdout(process::Stdio::piped())
            .stderr(process::Stdio::piped())
            .spawn()?;
        let stderr = if self.async_stderr {
            StderrReader::r#async(child.stderr.take().unwrap())
        } else {
            StderrReader::sync(child.stderr.take().unwrap())
        };
        Ok(CommandReader { child, stderr, eof: false })
    }

    pub fn async_stderr(&mut self, yes: bool) -> &mut CommandReaderBuilder {
        self.async_stderr = yes;
        self
    }
}
