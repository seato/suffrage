/*
 * Title:  suffrage2
 * Author:  Richard Thai and Garrick Merrill
 * Created:  12/16/2010
 * Modified:  2/25/2011
 *
 * Prerequisite: IMPORTANT!  The IXM's that download this code MUST have an
 * integer ID programmed in beforehand.  Use the IXM BIOS to set the board
 * NAME ((n=NAME) according to the help menu).  Note that the input is stored
 * on the boards in base-36 so that it can accept letters in addition to
 * integers (26 letters + 10 integers = 36 symbols).  You don't need an
 * 'integer' ID, but it's certainly a good idea since the IXM's are typically
 * shipped with a unique 8 digit identifying sticker.
 * Info on accessing the BIOS can be found in the Antipasto Hardware Wiki on
 * http://antipastohw.pbworks.com/w/page/30205779/IXM-Bios-Tutorial.
 *
 * Description:  This is a demonstration of the redundant qualities of the IXM.
 * Each board performs the same calculation (finding the n_th prime number) and
 * casts its answer as a vote within its interval-ping (heart-beat).  All votes
 * are distributed among the connected boards and a majority is determined with
 * every new vote.  Each board then recognizes the status of the grid (majority,
 * minority, or tie) and lights up its green, red or no LED respectively.  A
 * blue LED lights up to signify that it is currently processing (if you can
 * catch it, it's rather fast).
 *
 * Button Function:
 * To simulate incorrect answers, the button on
 * the IXM can be pressed the adjust what the affected IXM calculates.  In this
 * case, the "incorrect" calculation is for the (n + 1)_th prime instead of the
 * n_th prime.
 *
 * Terminal Commands:
 * >> x         - force a reboot to all IXM's in the grid
 * >> t         - request to stop sending heart-beat packets (starts with "r"
 *                followed by a combination of numbers and commas) and to start
 *                displaying the local IXM's internal table which will update on
 *                an interval
 * >> cN        - request to initiate a calculation for the N_th prime to all
 *                IXM's within the grid.  The vote for said calculation will be
 *                broadcast the moment it is generated.  Replace the 'N' with
 *                the N_th prime to calculate, for example:  c15 is a request to
 *                calculate the 15th prime.  There is a limit programmed within
 *                the boards as to how high of a prime they can take which can be
 *                found within the header file as "PRIME_THRESHOLD" and
 *                "PRIME_ARR_THRESHOLD" which is the prime sequence and the prime
 *                value plus one, respectively.  Change those for higher primes!
 *
 * Notes:
 * The boards do not rely on a slave/master setup, meaning that every internal
 * could potentially be different.  A weakness is that IXM(s) in the grid could
 * have discordant data.  However, this setup also allows IXM's to have no single
 * point of failure should an arbitrary board encounter a data/network fault
 * (assuming grid with redundant networking paths).
 * As the amount of IXM's increase, data and networking redundancy increase:
 *  - 2+ boards allow a consensus to be made
 *  - 3+ boards allow an incorrect vote to be determined
 *  - 4+ boards (in grid formation) ensure that a single failed board will
 *    not disrupt data paths for remaining boards
 */

#include "sketch.h"

/*
 * Summary:     Turns on a specific LED color depending on the status input.
 * Parameters:  u32 LED status denoting which LED to turn on.
 *              OFF = off, MINORITY = red, MAJORITY = green, PROCESSING = blue
 * Return:      None.
 */
void
setStatus(u32 STATUS)
{
  if ((STATUS != OFF) && // if input is not "no LED"
      (STATUS != MINORITY) && // or "red LED"
      (STATUS != MAJORITY) && // or "green LED"
      (STATUS != PROCESSING)) // or "blue LED"
    {
      logNormal("setStatus:  Invalid input %d\n", STATUS);
      // blinkcode!
      API_ASSERT((OFF == STATUS) || (MINORITY == STATUS)
          || (MAJORITY == STATUS) || (PROCESSING == STATUS), E_API_EQUAL);
    }

  for (u8 i = 0; i < 3; ++i)
    ledOff(LED_PIN[i]); // turn off the LEDs

  if (STATUS == OFF) // and if the status to display is "off"
    return; // keep the LEDs off

  ledOn(LED_PIN[STATUS]); // otherwise, turn on the appropriate LED

  return;
}

/*
 * Summary:     Clears out relevant variables arrays for a new calculation.
 * Parameters:  None.
 * Return:      None.
 */
void
flush()
{
  // reinitialize the global variables
  MAJORITY_RSLT = 0;
  CANDIDATE_COUNT = 0;
  VOTE_COUNT = 0;

  for (u32 i = 0; i < 32; ++i)
    { // and reinitialize the global arrays
      VOTE_NODE_ARR[i] = 0;
      CANDIDATE_ARR[i] = 0;
      CANDIDATE_VOTES_ARR[i] = 0;
    }

  setStatus(OFF); // reset the LED as well

  return;
}

/*
 * Summary:     Given an array and a specific range [0, size) where the "size"_th
 *              iteration is not inclusive, return the iteration of the first
 *              instance of the specified key, or -1 if it does not exist.
 * Parameters:  u32 array to search, u32 size of the array, and the u32 element
 *              to locate.
 * Return:      Iteration of the first location of the key, -1 if it doesn't
 *              exist.
 */
u32
linearSearch(u32 a[], u32 size, u32 key)
{
  if (NULL == a) // null array pointer?
    {
      logNormal("linearSearch:  NULL array pointer!\n");
      API_ASSERT_NONNULL(a); // blinkcode!
    }

  if (size <= 0) // invalid size?
    {
      logNormal("linearSearch:  Invalid size %d\n", size);
      API_ASSERT_GREATER(size, 0); // blinkcode!
    }

  if (key < 0) // invalid key?
    {
      logNormal("linearSearch:  Invalid size %d\n", size);
      API_ASSERT_GREATER_EQUAL(size, 0); // blinkcode!
    }

  for (u32 i = 0; i < size; ++i)
    if (a[i] == key) // If there is a match
      return i; // say so

  return INVALID; // Otherwise, return the signal that there is no match
}

/*
 * Summary:     Given an array and a specific range [first, size) where the
 *              "size"_th iteration is not inclusive, return the index of the
 *              greatest positive value in the array or INVALID if there is a
 *              tie.
 * Parameters:  u32 array to search, u32 size of the array.
 * Return:      Index of the highest value greater than or equal to 0;
 *              INVALID otherwise.
 */
u32
getMaxIndex(u32 a[], u32 size)
{
  if (NULL == a) // null array pointer?
    {
      logNormal("getMaxIndex:  NULL array pointer!\n");
      API_ASSERT_NONNULL(a); // blinkcode!
    }

  if (size <= 0) // invalid size?
    {
      logNormal("getMaxIndex:  Invalid size %d\n", size);
      API_ASSERT_GREATER(size, 0); // blinkcode!
    }

  bool tiedetected = false; // flag to mark if there is a tie
  u32 maxVotes = 0; // highest amount of ballots currently
  u32 maxIndex = INVALID; // index of the highest candidate

  for (u32 i = 0; i < size; ++i)
    { // iterate through the entire array
      if (a[i] > maxVotes)
        { // record higher values
          maxVotes = a[i];
          maxIndex = i;
          tiedetected = false; // disarm tie flag
        }
      // if there is ever candidate that ties with the highest candidate
      else if (a[i] == maxVotes)
        tiedetected = true; // set tie flag
    }

  if (tiedetected) // if the tie flag was set
    return TIE; // there is no winning candidate

  // It worked, there's a candidate greater than 0 votes and no ties
  else if ((0 != maxVotes) && (INVALID != maxIndex))
    return maxIndex;

  else
    return INVALID; // This should never happen.
}

/*
 * Summary:     Custom (c)alculation packet scanner.
 * Parameters:  The arguments are automatically handled within a parent
 *              header file.
 * Return:      Boolean confirming the packet was read correctly.
 */
bool
C_ZScanner(u8 * packet, void * arg, bool alt, int width)
{
  /* (c)alculation packet structure */
  u32 CALC; // integer calculation

  if (packetScanf(packet, "%d", &CALC) != 1)
    {
      logNormal("Inconsistent packet format for (c)alculation packet.\n");

      return false;
    }

  if (arg)
    {
      C_PKT * PKT_R = (C_PKT*) arg;
      PKT_R->calc = CALC;
    }

  return true;
}

/*
 * Summary:     Custom (r)esult packet printer.
 * Parameters:  The arguments listed are automatically handled within a
 *              parent header file.
 * Return:      None.
 */
void
R_ZPrinter(u8 face, void * arg, bool alt, int width, bool zerofill)
{
  API_ASSERT_NONNULL(arg);

  R_PKT PKT_T = *(R_PKT*) arg;

  facePrintf(face, "%t,%d,%d,%d,%d,%d", PKT_T.key.ID, PKT_T.key.TIME,
      PKT_T.calc, PKT_T.calc_ver, PKT_T.rslt, PKT_T.neighbor);

  return;
}

/*
 * Summary:     Custom (r)esult packet scanner.
 * Parameters:  The arguments are automatically handled within a parent
 *              header file.
 * Return:      Boolean confirming the packet was read correctly.
 */
bool
R_ZScanner(u8 * packet, void * arg, bool alt, int width)
{
  /* (r)esult packet structure */
  u32 ID; // hex ID (board key)
  u32 TIME; // integer timestamp (packet key)
  u32 CALC; // integer calculation
  u32 CALC_VER; // integer calculation version
  u32 VOTE; // integer vote
  u32 NEIGHBOR; // neighbor flag

  if (packetScanf(packet, "%t,%d,%d,%d,%d,%d", &ID, &TIME, &CALC, &CALC_VER,
      &VOTE, &NEIGHBOR) != 11)
    {
      logNormal("Inconsistent packet format for (r)esult packet.\n");
      return false;
    }

  if (arg)
    {
      R_PKT * PKT_R = (R_PKT*) arg;
      PKT_R->key.ID = ID;
      PKT_R->key.TIME = TIME;
      PKT_R->calc = CALC;
      PKT_R->calc_ver = CALC_VER;
      PKT_R->rslt = VOTE;
      PKT_R->neighbor = NEIGHBOR;
    }

  return true;
}

/*
 * Summary:     Broadcasts the received packet to the neighboring nodes
 *              save for the terminal face if it is known.
 * Parameters:  (r)esult packet to be broadcasted.
 * Return:      None.
 */
void
BRD_R_PKT(struct R_PKT *PKT_T)
{
  for (u32 i = 0; i < 4; ++i) // Forward received packet to neighboring nodes
    if (TERMINAL_FACE != i) // but don't forward to the terminal face
      facePrintf(i, "r%Z%z\n", R_ZPrinter, PKT_T);

  return;
}

/*
 * Summary:     Forwards the received packet to the neighboring nodes
 *              save for the terminal face if known and the receiving face.
 * Parameters:  (r)esult packet to be forwarded.
 * Return:      None.
 */
void
FWD_R_PKT(struct R_PKT *PKT_T, u8 face)
{
  for (u32 i = 0; i < 4; ++i) // Forward received packet to neighboring nodes
    if ((TERMINAL_FACE != i) && (face != i)) // that aren't the terminal or source face
      facePrintf(i, "r%Z%z\n", R_ZPrinter, PKT_T);

  return;
}

/*
 * Summary:     Alarm to reboot a board
 * Parameters:  Time when function was called (handled automagically).
 * Return:      None.
 */
void
reboot(u32 when)
{
  for (u32 i = 0; i < FACE_COUNT; ++i)
    if (REBOOT_ARR[i])
      {
        powerOut(REBOOT_ARR[i], 1);
        REBOOT_ARR[i] = 0;
      }

  return;
}

/*
 * Summary:     Checks to see if the given node is a neighbor.
 * Parameters:  u32 ID.
 * Return:      face, or INVALID if it's not a neighbor.
 */
u32
getNeighborFace(u32 id)
{
  for (u32 i = 0; i < FACE_COUNT; ++i) // Look through the array
    if (id == NEIGHBORS_ARR[i]) // If we find a match
      return i; // Say it's face

  return INVALID; // Otherwise, it's not a neighboring node
}

/*
 * Summary:     Swiftly looks to see what IXM's have given an incorrect answer and
 *              updates the tables accordingly.
 * Parameters:  None.
 * Return:      None.
 */
void
strikeCheck()
{
  u32 face;

  for (u32 i = 0; i < NODE_COUNT; ++i)
    {
      if (VOTE_NODE_ARR[i] != MAJORITY_RSLT)
        ++STRIKES_NODE_ARR[i];
      else
        STRIKES_NODE_ARR[i] = 0;

      face = getNeighborFace(ID_NODE_ARR[i]);

      if ((STRIKES_NODE_ARR[i] > 2) && (INVALID != face))
        {
          powerOut(face, 0);
          REBOOT_ARR[face] = 1;

          Alarms.set(Alarms.create(reboot), millis() + reboot_PERIOD); // Set a reboot
          return;
        }
    }
}

/*
 * Summary:     Logs the ID and time-stamp keys of a received packet.
 * Parameters:  u32 ID, u32 time-stamp (from a (r)esult packet)
 * Return:      Return index of the node if successful, otherwise INVALID.
 */
u32
log(u32 ID, u32 TIME)
{
  if (TIME < 0)
    {
      logNormal("log:  No time-traveling or overflow allowed!\n");
      API_ASSERT_GREATER_EQUAL(TIME, 0); // blinkcode!
    }

  for (u32 i = 0; i < NODE_COUNT; ++i)
    { // Look for an existing match in the list of previous PING'ers
      if (ID == ID_NODE_ARR[i])
        {
          if (TIME != TS_NODE_ARR[i])
            { // If there is a match and it is a new packet
              if (PC_NODE_ARR[i] < 0xffff) // Make sure ping count won't overflow
                ++PC_NODE_ARR[i]; // So we can keep track of the valid packet
              else
                // Notify us if the ping count will overflow
                logNormal("Limit of pings reached for IXM %t\n", ID);

              TS_NODE_ARR[i] = TIME; // Update nodular time-stamp
              TS_HOST_ARR[i] = millis(); // Update host-based time-stamp

              return i; // Return the location of the existing node
            }

          else
            // Don't forward the packet if it isn't new
            return INVALID;
        }
    }

  // Otherwise check to see if there is any free space left in the array
  if (NODE_COUNT >= (sizeof(ID_NODE_ARR) / sizeof(u32)))
    {
      logNormal("Inadequate memory space in ID table.\n"
        "Rebooting.\n");
      reenterBootloader(); // Clear out memory for new boards
    }

  // Add the new IXM board to the phone-book.
  ID_NODE_ARR[NODE_COUNT] = ID;
  TS_NODE_ARR[NODE_COUNT] = TIME;
  TS_HOST_ARR[NODE_COUNT] = millis();
  ++PC_NODE_ARR[NODE_COUNT];

  return NODE_COUNT++; // And pass it on
}

/*
 * Summary:     Evaluates the votes for all of the candidates and changes the
 *              RESULT (global variable for majority) appropriately.
 * Parameters:  None.
 * Return:      None.
 */
void
evalMajority()
{
  if (VOTE_COUNT >= VOTE_COUNT_MIN) // If there are at least 2 active nodes
    {
      if (0 == HOST_CALC) // Consider 0 an invalid calculation
        {
          flush(); // and reset everything
          return;
        }

      // Look to see the location of the candidate with the most votes
      u32 j = getMaxIndex(CANDIDATE_VOTES_ARR, (CANDIDATE_COUNT + 1));

      if (TIE == j) // turn off the LED's in a tie-case
        {
          MAJORITY_RSLT = TIE; // set the tie as an indicator
          setStatus(OFF);
          return;
        }

      else // if there was no tie
        {
          MAJORITY_RSLT = CANDIDATE_ARR[j]; // set majority accordingly
          setStatus((VOTE_NODE_ARR[0] == CANDIDATE_ARR[j]) ? MAJORITY
              : MINORITY); // set host LED based off of agreement.
          return;
        }
    }

  else
    setStatus(OFF); // Inadequate amount of IXM's for voting

  return;
}

/*
 * Summary:     Currently generates the n_th prime number using the well-known
 *              "Sieve of Atkins" algorithm.  More information and explanations
 *              on the algorithm can be found online.
 *
 *              The nice part of this function is that a different calculation
 *              can be swapped in for voting pretty easily without changing too
 *              many things in the architecture.  If you want to add more
 *              arguments/results, then adjust the (r)esult and (c)alculation
 *              packet structures accordingly.
 * Parameters:  n - the prime sequence to generate.
 * Return:      The n_th prime number.
 */
u32
calculate(u32 a)
{
  if (a < 1) // invalid calculation
    {
      flush(); // take it as a signal to clear out the boards
      setStatus(OFF); // and to turn off the LED
      return 0;
    }

  setStatus(PROCESSING); // blue LED indicates calculation

  u32 b;
  u32 c;
  u32 i;
  u32 j;
  u32 k;

  // This adds the faulty factor into the calculation
  if (FAULTY) // If the button was pressed an odd amount of times
    b = a + 1; // (n + 1)_th prime
  else
    // otherwise
    b = a; // n_th prime

  // three embedded FOR-loops = efficiency hell
  for (i = 3; i < PRIME_ARR_THRESHOLD; ++i)
    {
      c = 0;

      for (i = 0; i < PRIME_ARR_THRESHOLD; ++i)
        sieve[i] = 0;

      for (j = 2; j * j <= i; j++)
        if (!sieve[j])
          for (k = j + j; k < i; k += j)
            sieve[k] = 1;

      for (j = 2; j < i; j++)
        {
          if (!sieve[j])
            ++c;
          if (b == c)
            return j;
        }
    }

  return 0;
}

/*
 * Summary:     Adjusts the vote count based on a recent vote.
 * Parameters:  u32 index of the node that voted, u32 ballot of the node.
 * Return:      None.
 */
void
voteCount(u32 NODE_INDEX, u32 BALLOT)
{
  if (NODE_INDEX < 0)
    {
      logNormal("voteCount:  Invalid node index %d", NODE_INDEX);
      API_ASSERT_GREATER_EQUAL(NODE_INDEX, 0); // blinkcode!
    }

  if (BALLOT < 0)
    {
      logNormal("voteCount:  Invalid ballot %d", BALLOT);
      API_ASSERT_GREATER_EQUAL(BALLOT, 0); // blinkcode!
    }

  if (0 == BALLOT) // 0 is never a correct answer
    return; // But it's not worth blinkcoding over

  else if (BALLOT == VOTE_NODE_ARR[NODE_INDEX])
    return; // Ignore duplicate votes

  // Invalid vote if I've already voted (didn't flush from a new calculation)
  else if (0 != VOTE_NODE_ARR[NODE_INDEX])
    return;

  else if (0 == VOTE_NODE_ARR[NODE_INDEX]) // Haven't voted yet
    ++VOTE_COUNT; // This is the first vote received

  if (VOTE_COUNT > NODE_COUNT) // Someone voted multiple times
    {
      flush(); // Clean out memory
      voteCount(0, calculate(HOST_CALC)); // Recount our own vote
      return;
    }

  // look to see if the ballot is for an existing candidate
  u32 k = linearSearch(CANDIDATE_ARR, CANDIDATE_COUNT + 1, BALLOT);

  if (INVALID != k) // if so, give him a vote
    ++CANDIDATE_VOTES_ARR[k];

  else // otherwise the ballot is new to the candidate list
    { // so add it
      k = CANDIDATE_COUNT++; // Give him a vote
      CANDIDATE_ARR[k] = BALLOT; // Add the new candidate to the nominations list
      ++CANDIDATE_VOTES_ARR[k]; // Give the candidate a vote
    }

  VOTE_NODE_ARR[NODE_INDEX] = BALLOT; // record the node's ballot

  evalMajority(); //Reevaluate the majority with every different ballot

  return;
}

/*
 * Summary:     Handles (r)esult packet reflex.  Packet information is logged and
 *              result is logged for the specific calculation.
 * Parameters:  (r)esult packet.
 * Return:      None.
 */
void
r_handler(u8 * packet)
{
  R_PKT PKT_R;

  if (packetScanf(packet, "%Zr%z\n", R_ZScanner, &PKT_R) != 3)
    {
      logNormal("r_handler:  Failed at %d\n", packetCursor(packet));
      return; // No harm done so no blinkcoding necessary
    }

  u32 NODE_INDEX; // index holder for if log is valid

  // only log properly formatted packets
  if (INVALID == (NODE_INDEX = log(PKT_R.key.ID, PKT_R.key.TIME)))
    return; // Don't continue if this packet has been received before

  // Handle packet spammers
  else if (PC_NODE_ARR[NODE_INDEX] > (PKT_R.key.TIME / 1000))
    {
      // Decrease the amount of pings recorded; "spammer amnesty" of sorts.
      PC_NODE_ARR[NODE_INDEX] -= 2;
      return; // Don't continue if this IXM is spamming packets right now.
    }

  else if (PKT_R.calc_ver < HOST_CALC_VER)
    return; // Don't continue if this is an old calculation version

  else if (0xffffffff == PKT_R.calc_ver)
    logNormal("Calculation version overflow.\n");

  if (PKT_R.neighbor)
    { // If this a neighboring node
      PKT_R.neighbor = 0; // Reset the flag before forwarding
      NEIGHBORS_ARR[packetSource(packet)] = PKT_R.key.ID; // And remember the ID
    }

  // If all the hoops have been jumped through
  FWD_R_PKT(&PKT_R, packetSource(packet)); // Forward the packet

  if (PKT_R.calc_ver == HOST_CALC_VER) // Same result?
    // Update the results from packets with proper calculation versions
    voteCount(NODE_INDEX, PKT_R.rslt);

  else if (PKT_R.calc_ver > HOST_CALC_VER) // New calculation version?
    { // perform standard procedures
      strikeCheck(); // evaluate the strikes for my neighbors
      flush(); // clear out my records for the new voting session
      voteCount(NODE_INDEX, PKT_R.rslt); // Remember the node's vote
      HOST_CALC = PKT_R.calc; // Remember the new calculation
      HOST_CALC_VER = PKT_R.calc_ver; // Remember the new calculation version
      voteCount(0, calculate(HOST_CALC)); // calculation occurs here
    }

  return;
}

/*
 * Summary:     Handles (c)alculation packet reflex.  Packet information is saved
 *              and converted into a R packet to be forwarded to neighboring nodes.
 * Parameters:  (c)alculation packet.
 * Return:      None.
 */
void
c_handler(u8 * packet)
{
  C_PKT PKT_R;

  if (packetScanf(packet, "%Zc%z\n", C_ZScanner, &PKT_R) != 3)
    {
      logNormal("c_handler:  Failed at %d\n", packetCursor(packet));
      return;
    }

  if (PKT_R.calc < 0)
    {
      logNormal("c_handler:  Input %d must be a non-negative integer.\n",
          PKT_R.calc);
      return;
    }

  if (PKT_R.calc > PRIME_THRESHOLD)
    {
      logNormal("c_handler:  Value %d is higher than the threshold %d (%d).\n",
          PKT_R.calc, PRIME_THRESHOLD, PRIME_ARR_THRESHOLD - 1);
      return;
    }

  strikeCheck(); // evaluate the strikes for my neighbors
  flush(); // clear out my records for the new voting session

  R_PKT PKT_T; // synthesize a new packet
  ++HOST_CALC_VER;
  HOST_CALC = PKT_R.calc;
  PKT_T.key.ID = ID_HOST;
  PKT_T.key.TIME = millis();
  PKT_T.calc = HOST_CALC;
  PKT_T.calc_ver = HOST_CALC_VER;
  PKT_T.rslt = VOTE_NODE_ARR[0];

  // If all the hoops have been jumped through
  FWD_R_PKT(&PKT_T, packetSource(packet)); // Forward the packet
  voteCount(0, calculate(HOST_CALC)); // calculation occurs here

  return;
}

/*
 * Summary:     Displays a table containing each IXM's ID, timestamp(ms), and
 *              pings.  Note that there might exist a time-stamp inconsistency
 *              (boards may base their time based on when they started receiving
 *              power).
 * Parameters:  Time when function was called (handled automagically).
 * Return:      None.
 */
void
printTable(u32 when)
{
  if (when < 0)
    {
      logNormal("log:  No time-traveling or overflow allowed!\n");
      API_ASSERT_GREATER_EQUAL(when, 0); // blinkcode!
    }

  u32 HOST_TIME = when;

  facePrintf(
      TERMINAL_FACE,
      "\n\n\n\n\n\n\n\n\n\n\n\n\n+===============================================================+\n");
  facePrintf(TERMINAL_FACE,
      "|CALCULATION: %4d     HOST TIME: %010d                    |\n",
      HOST_CALC, HOST_TIME);
  facePrintf(TERMINAL_FACE,
      "+---------------------------------------------------------------+\n");
  facePrintf(TERMINAL_FACE,
      "|ID       ACTIVE     TIME-STAMP     VOTE       STRIKES     PINGS|\n");
  facePrintf(TERMINAL_FACE,
      "+----     ------     ----------     ------     -------     -----+\n");
  for (u32 i = 0; i < NODE_COUNT; ++i)
    {
      facePrintf(TERMINAL_FACE, "|%04t          %c%15d%11d%12d%10d|\n",
          ID_NODE_ARR[i], ACTIVE_NODE_ARR[i], TS_HOST_ARR[i], VOTE_NODE_ARR[i],
          STRIKES_NODE_ARR[i], PC_NODE_ARR[i]);
    }
  facePrintf(TERMINAL_FACE,
      "+---------------------------------------------------------------+\n");
  if ((TIE == MAJORITY_RSLT) || (INVALID == MAJORITY_RSLT) || (0
      == MAJORITY_RSLT))
    facePrintf(TERMINAL_FACE,
        "|MAJORITY: --                                                   |\n");
  else
    facePrintf(TERMINAL_FACE,
        "|MAJORITY: %4d                                                 |\n",
        MAJORITY_RSLT);
  facePrintf(TERMINAL_FACE,
      "+===============================================================+\n");

  // schedule the next table printout
  Alarms.set(Alarms.currentAlarmNumber(), when + printTable_PERIOD);

  return;
}

/*
 * Summary:     Sets a table-printing alarm that will reset itself on interval.
 * Parameters:  (t)able packet.
 * Return:      None.
 */
void
t_handler(u8 * packet)
{
  TERMINAL_FACE = packetSource(packet); // remember where this request came from
  Alarms.set(Alarms.create(printTable), millis()); // schedule the first table

  return;
}

/*
 * Summary:     Handles (x) packet reflex:  Reboot signal.
 * Parameters:  'x' packet.
 * Return:      None.
 */
void
x_handler(u8 * packet)
{
  if (packetScanf(packet, "x\n") != 2)
    return;

  facePrintln(ALL_FACES, "x"); // Be indiscrimnate to all faces
  delay(500); // Give some time for the action to be performed
  reenterBootloader(); // Clear out memory for new boards

  return; // Never actually returns
}

/*
 * Summary:     Sends an r packet containing basic info to all faces on interval
 *              and evaluates activity/inactivity status of boards.
 * Parameters:  Time when function was called (handled automagically).
 * Return:      None.
 */
void
heartBeat(u32 when)
{
  // synthesize a new packet
  R_PKT PKT_T;

  PKT_T.key.ID = ID_HOST;
  PKT_T.key.TIME = millis();
  PKT_T.calc = HOST_CALC;
  PKT_T.calc_ver = HOST_CALC_VER;
  PKT_T.rslt = VOTE_NODE_ARR[0];
  PKT_T.neighbor = 1;

  if (PC_NODE_ARR[0] > (PKT_T.key.TIME / 1000)) // Spam self-safeguard
    {
      PC_NODE_ARR[0] -= 2; // self "spammer amnesty"
      return;
    }

  BRD_R_PKT(&PKT_T); // broadcast the packet
  ++PC_NODE_ARR[0]; // update recent host ping count
  TS_NODE_ARR[0] = PKT_T.key.TIME; // update recent host ping times
  TS_HOST_ARR[0] = PKT_T.key.TIME;

  for (u32 i = 1; i < NODE_COUNT; ++i)
    { // evaluate non-host IXM activity/inactivity
      ACTIVE_NODE_ARR[i] = (((TS_HOST_ARR[0] - TS_HOST_ARR[i]) < IDLE) ? 'A'
          : 'I'); // For displaying activity/inactivity on the table

      if ('I' == ACTIVE_NODE_ARR[i])
        { // If a board goes inactive
          PC_NODE_ARR[i] = 0; // Reset their pings
          STRIKES_NODE_ARR[i] = 0; // And their strike counts
        }
    }

  // Schedule the next heartbeat
  Alarms.set(Alarms.currentAlarmNumber(), when + pingAll_PERIOD);

  return;
}

/*
 * Summary:     Signals whether this specific board is set to give a faulty result
 *              3 flashes with half-second interval;
 *              GREEN=>non-faulty, RED=>faulty.
 * Parameters:  u32 led status
 *              BODY_RGB_GREEN_PIN = green, BODY_RGB_RED_PIN = red
 * Return:      None.
 */
void
faultSignal(u32 STATUS_LED)
{
  bool LED_PIN_STATE[3] =
    { false }; // place-holder for remembering which LEDs were on

  for (u32 i = 0; i < 3; ++i)
    { // Preserve the state of the LED lights
      LED_PIN_STATE[i] = ledIsOn(LED_PIN[i]);
      ledOff(LED_PIN[i]); // Turn off the light
    }

  for (u32 j = 0; j < 3; ++j)
    { // Flash thrice if faulty or not
      ledOn(STATUS_LED);
      delay(FAULT_STATUS_PERIOD);

      ledOff(STATUS_LED);
      delay(FAULT_STATUS_PERIOD);
    }

  for (u32 k = 0; k < 3; ++k)
    if (LED_PIN_STATE[k])
      ledOn(LED_PIN[k]); // Restore the state of the LED lights

  return;
}

/*
 * Summary:     Toggles the FAULTY flag for determining a faulty/non-faulty result.
 * Parameters:  None.
 * Return:      None.
 */
void
faultToggle()
{
  FAULTY = !(FAULTY); // Toggle whether the calculation will be faulty or non-faulty
  faultSignal((FAULTY ? BODY_RGB_RED_PIN : BODY_RGB_GREEN_PIN)); // Flashing signal

  return;
}

/*
 * Summary:     Initialization.
 * Parameters:  None.
 * Return:      None.
 */
void
setup()
{
  // Initialize reflexes
  Body.reflex('r', r_handler);
  Body.reflex('c', c_handler);
  Body.reflex('t', t_handler);
  Body.reflex('x', x_handler);

  // Initialize host values
  ID_NODE_ARR[0] = ID_HOST;
  ACTIVE_NODE_ARR[0] = 'A';

  Alarms.set(Alarms.create(heartBeat), pingAll_PERIOD); // Start the heartbeats
  faultSignal((FAULTY ? BODY_RGB_RED_PIN : BODY_RGB_GREEN_PIN)); // HE LIVES!

  return;
}

/*
 * Summary:     Puts IXM in a "listening state".
 * Parameters:  None.
 * Return:      None.
 */
void
loop()
{
  for (u32 i = 0; i < 10; ++i)
    { // Button press debouncer for toggling the FAULTY flag
      delay(1);
      if (!buttonDown())
        i = 0;
    }

  faultToggle(); // Toggle the FAULTY flag!

  for (u32 i = 0; i < 10; ++i)
    {
      delay(1); // Button depress debouncer for toggling the FAULTY flag
      if (buttonDown())
        i = 0;
    }

  return;
}

#define SFB_SKETCH_CREATOR_ID B36_4(n,a,s,a)
#define SFB_SKETCH_PROGRAM_ID B36_6(s,u,f,r,g,e)
