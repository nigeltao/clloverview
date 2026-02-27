// https://github.com/BurntSushi/ripgrep/blob/master/crates/searcher/src/sink.rs
// with further reductions. We just need syntax examples, not full code.

pub mod sinks {
    #[derive(Clone, Debug)]
    pub struct UTF8<F>(pub F)
    where
        F: FnMut(u64, &str) -> Result<bool, io::Error>;

    impl<F> Sink for UTF8<F>
    where
        F: FnMut(u64, &str) -> Result<bool, io::Error>,
    {
        type Error = io::Error;

        fn matched(
            &mut self,
            _searcher: &Searcher,
            mat: &SinkMatch<'_>,
        ) -> Result<bool, io::Error> {
            // Etc.
        }
    }
}
