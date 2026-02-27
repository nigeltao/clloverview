// https://github.com/BurntSushi/ripgrep/blob/master/crates/matcher/src/lib.rs
// with further reductions. We just need syntax examples, not full code.

impl std::ops::Index<Match> for [u8] {
    type Output = [u8];

    #[inline]
    fn index(&self, index: Match) -> &[u8] {
        &self[index.start..index.end]
    }
}

impl std::ops::IndexMut<Match> for [u8] {
    #[inline]
    fn index_mut(&mut self, index: Match) -> &mut [u8] {
        &mut self[index.start..index.end]
    }
}

impl std::ops::Index<Match> for str {
    type Output = str;

    #[inline]
    fn index(&self, index: Match) -> &str {
        &self[index.start..index.end]
    }
}

// ----

pub struct NoError(());

impl std::error::Error for NoError {
    fn description(&self) -> &str {
        "no error"
    }
}

impl std::fmt::Display for NoError {
    fn fmt(&self, _: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        panic!("BUG for NoError: an impossible error occurred")
    }
}

impl From<NoError> for std::io::Error {
    fn from(_: NoError) -> std::io::Error {
        panic!("BUG for NoError: an impossible error occurred")
    }
}

// ----

impl<'a, M: Matcher> Matcher for &'a M {
    type Captures = M::Captures;
    type Error = M::Error;

    #[inline]
    fn find_at(
        &self,
        haystack: &[u8],
        at: usize,
    ) -> Result<Option<Match>, Self::Error> {
        (*self).find_at(haystack, at)
    }

    #[inline]
    fn new_captures(&self) -> Result<Self::Captures, Self::Error> {
        (*self).new_captures()
    }
}
