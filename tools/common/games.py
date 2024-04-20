# -*- coding: utf-8 -*-

import re
from collections import defaultdict
from dataclasses import dataclass
from typing      import Any, Generator, Iterable, Mapping, Sequence

## Definitions

# Character 0:    always G
# Character 1:    region related? (can be B, C, E, K, L, N, Q, U, X or wildcard)
# Characters 2-4: identifier (700-999 or A00-A99 ~ D00-D99)
GAME_CODE_REGEX: re.Pattern = \
	re.compile(rb"G[A-Z*][0-9A-D][0-9][0-9]", re.IGNORECASE)

# Character 0:    region (A=Asia?, E=Europe, J=Japan, K=Korea, S=?, U=US)
# Character 1:    type/variant (A-F=regular, R-W=e-Amusement, X-Z=?)
# Characters 2-4: game revision (A-D or Z00-Z99, optional)
GAME_REGION_REGEX: re.Pattern = \
	re.compile(rb"[AEJKSU][A-FR-WX-Z]([A-D]|Z[0-9][0-9])?", re.IGNORECASE)

_CARTS_WITH_ID: Sequence[str] = (
	"X76F041+DS2401",
	"ZS01+DS2401"
)

_IO_BOARDS_WITH_ID: Sequence[str] = (
	"GX700-PWB(K)", # Kick & Kick expansion board
	"GX894-PWB(B)", # Digital I/O board
	"GX921-PWB(B)", # DDR Karaoke Mix expansion board
	"PWB0000073070" # GunMania expansion board
)

## Game list (loaded from games.json)

@dataclass
class GameDBEntry:
	code:   str
	region: str
	name:   str

	mameID:          str | None = None
	installCart:     str | None = None
	gameCart:        str | None = None
	ioBoard:         str | None = None

	cartLockedToIOBoard:  bool = False
	flashLockedToIOBoard: bool = False

	# Implement the comparison overload so sorting will work. The 3-digit number
	# in the game code is used as a key.
	def __lt__(self, entry: Any) -> bool:
		return ( self.code[2:], self.code[0:2], self.region, self.name ) < \
			( entry.code[2:], entry.code[0:2], entry.region, entry.name )

	def __str__(self) -> str:
		return f"{self.code} {self.region}"

	def getFullName(self) -> str:
		return f"{self.name} [{self.code} {self.region}]"

	def hasCartID(self) -> bool:
		if self.gameCart is None:
			return False

		return (self.gameCart in _CARTS_WITH_ID)

	def hasSystemID(self) -> bool:
		return (self.ioBoard in _IO_BOARDS_WITH_ID)

class GameDB:
	def __init__(self, entries: Iterable[Mapping[str, Any]] | None = None):
		self._idIndex:   dict[str, GameDBEntry]              = {}
		self._codeIndex: defaultdict[str, list[GameDBEntry]] = defaultdict(list)

		if entries:
			for entry in entries:
				self.addEntry(entry)

	def addEntry(self, entryObj: Mapping[str, Any]):
		code:   str = entryObj["code"].strip().upper()
		region: str = entryObj["region"].strip().upper()
		name:   str = entryObj["name"]

		mameID:          str | None = entryObj.get("id",              None)
		installCart:     str | None = entryObj.get("installCart",     None)
		gameCart:        str | None = entryObj.get("gameCart",        None)
		ioBoard:         str | None = entryObj.get("ioBoard",         None)

		cartLockedToIOBoard:  bool = entryObj.get("cartLockedToIOBoard",  False)
		flashLockedToIOBoard: bool = entryObj.get("flashLockedToIOBoard", False)

		if GAME_CODE_REGEX.fullmatch(code.encode("ascii")) is None:
			raise ValueError(f"invalid game code: {code}")
		if GAME_REGION_REGEX.fullmatch(region.encode("ascii")) is None:
			raise ValueError(f"invalid game region: {region}")

		entry: GameDBEntry = GameDBEntry(
			code, region, name, mameID, installCart, gameCart, ioBoard,
			cartLockedToIOBoard, flashLockedToIOBoard
		)

		if mameID is not None:
			self._idIndex[mameID.lower()] = entry

		# Store all entries indexed by their game code and first two characters
		# of the region code. This allows for quick retrieval of all revisions
		# of a game.
		self._codeIndex[f"{code}{region[0:2]}"].append(entry)
		self._codeIndex[f"{code[0]}*{code[2:]}{region[0:2]}"].append(entry)

	def lookupByID(self, mameID: str) -> GameDBEntry:
		return self._idIndex[mameID.lower()]

	def lookupByCode(
		self, code: str, region: str
	) -> Generator[GameDBEntry, None, None]:
		_code:   str = code.strip().upper()
		_region: str = region.strip().upper()

		# If only two characters of the region code are provided, match all
		# entries whose region code starts with those two characters (even if
		# longer).
		for entry in self._codeIndex[_code + _region[0:2]]:
			if _region == entry.region[0:len(_region)]:
				yield entry
