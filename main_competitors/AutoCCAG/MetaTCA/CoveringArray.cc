#include "CoveringArray.h"

CoveringArray::CoveringArray(const SpecificationFile &specificationFile,
                             const ConstraintFile &constraintFile, 
                             const ConfigurationFile &configurationFile,
                            int seed, const int finalChk)
    : validater(specificationFile), satSolver(constraintFile.isEmpty()), specificationFile(specificationFile),
      coverage(specificationFile), configurationFile(configurationFile), finalCheck(finalChk) {
  specialRandomOn = checkInstance(constraintFile);
  clock_start = clock();
  const Options &options = specificationFile.getOptions();
  // add constraint into satSolver
  const std::vector<InputClause> &clauses = constraintFile.getClauses();
  for (unsigned i = 0; i < clauses.size(); ++i) {
    satSolver.addClause(const_cast<InputClause &>(clauses[i]));
  }
  const Valid::Formula &formula = constraintFile.getFormula();
#ifndef NVISIBLE
  formula.print();
#endif
  for (auto &c : formula) {
    validater.addClause(c);
  }
  option_constrained_indicator.clear();
  option_constrained_indicator.resize(options.size(), false);
  for (auto &c : formula) {
    for (auto &lit : c) {
      option_constrained_indicator[options.option(lit.variable())] = true;
    }
  }

  for (unsigned option = 0; option < options.size(); ++option) {
    if (!option_constrained_indicator[option]) {
      continue;
    }
    InputClause atLeast;
    for (unsigned j = options.firstSymbol(option),
                  limit = options.lastSymbol(option);
         j <= limit; ++j) {
      atLeast.append(InputTerm(false, j));
    }
    satSolver.addClause(atLeast);
    for (unsigned j = options.firstSymbol(option),
                  limit = options.lastSymbol(option);
         j <= limit; ++j) {
      for (unsigned k = j + 1; k <= limit; ++k) {
        InputClause atMost;
        atMost.append(InputTerm(true, j));
        atMost.append(InputTerm(true, k));
        satSolver.addClause(atMost);
      }
    }
  }

  // coverage.initialize(satSolver, option_constrained_indicator);
  // uncoveredTuples.initialize(specificationFile, coverage, true);
  //coverage.unconstrained_initialize();

  coverage.unconstrained_initialize();
  uncoveredTuples.initialize(specificationFile, coverage, false);


  

  step = 0;
  mersenne.seed(seed);
}

void CoveringArray::greedyConstraintInitialize() {
  oneCoveredTuples.initialize(specificationFile, array.size());
  for (auto encode : uncoveredTuples) {
    const std::vector<unsigned> tuple = coverage.getTuple(encode);
    for (auto var : tuple) {
      varInUncovertuples.insert(var);
    }
  }
  const Options &options = specificationFile.getOptions();
  unsigned width = options.size();

  while (uncoveredTuples.size()) {
    oneCoveredTuples.addLine(options.allSymbolCount());
    array.push_back(std::vector<unsigned>(width));

    // reproduce it randomly, with at least one tuple covered
    unsigned encode =
        uncoveredTuples.encode(mersenne.next(uncoveredTuples.size()));
    mostGreedySatRow(array.size() - 1, encode);
  }
  entryTabu.initialize(Entry(array.size(), array.size()));

  validater.initialize(array);
}

void CoveringArray::arrayInitialize(const std::string file_name) {
  const Options &options = specificationFile.getOptions();
  const unsigned &strenth = specificationFile.getStrenth();
  std::ifstream res_file(file_name);
  if (!res_file.is_open()) {
    std::cerr << "file open failed" << std::endl;
    exit(0);
  }
  std::string line;
  while (getline(res_file, line)) {
    array.push_back(std::vector<unsigned>(options.size()));
    std::vector<unsigned> &newRow = *array.rbegin();
    std::istringstream is(line);
    for (unsigned option = 0; option < options.size(); ++option) {
      unsigned value;
      is >> value;
      newRow[option] = value + options.firstSymbol(option);
    }
  }
  res_file.close();

  std::vector<size_t> coverByLineIndex(coverage.tupleSize());
  std::vector<unsigned> tuple(strenth);
  for (unsigned lineIndex = 0; lineIndex < array.size(); ++lineIndex) {
    auto &line = array[lineIndex];
    for (std::vector<unsigned> columns = combinadic.begin(strenth);
         columns[strenth - 1] < line.size(); combinadic.next(columns)) {
      for (unsigned i = 0; i < strenth; ++i) {
        tuple[i] = line[columns[i]];
      }
      //			cover(coverage.encode(columns, tuple));
      unsigned encode = coverage.encode(columns, tuple);
      coverByLineIndex[encode] = lineIndex;
      coverage.cover(encode);
    }
  }
  coverage.set_zero_invalid();
  entryTabu.initialize(Entry(array.size(), array.size()));
  validater.initialize(array);
  oneCoveredTuples.initialize(specificationFile, array.size());
  oneCoveredTuples.pushOneCoveredTuple(coverage, coverByLineIndex);
}


// void CoveringArray::actsInitialize(const std::string file_name) {
//   const Options &options = specificationFile.getOptions();
//   const unsigned &strenth = specificationFile.getStrenth();
//   std::ifstream res_file(file_name);
//   if (!res_file.is_open()) {
//     std::cerr << "file open failed" << std::endl;
//     exit(0);
//   }
//   std::string line;
//   while (getline(res_file, line)) {
//     if (line.find("Test Cases") != std::string::npos) {
//       break;
//     }
//   }
//   while (true) {
//     bool begin = false;
//     while (getline(res_file, line)) {
//       if (line[0] == '1') {
//         begin = true;
//         break;
//       }
//     }
//     if (!begin) {
//       break;
//     }
//     array.push_back(std::vector<unsigned>(options.size()));
//     std::vector<unsigned> &newRow = *array.rbegin();
//     for (unsigned option = 0; option < options.size(); ++option) {
//       unsigned value;
//       std::string value_str(
//           line.substr(line.find_last_of('=') + 1, line.size() - 1));
//       value = atoi(value_str.c_str());
//       newRow[option] = value + options.firstSymbol(option);
//       getline(res_file, line);
//     }
//   }
//   res_file.close();

//   clock_t start = clock();
//   std::vector<size_t> coverByLineIndex(coverage.tupleSize());
//   std::vector<unsigned> tuple(strenth);
//   for (unsigned lineIndex = 0; lineIndex < array.size(); ++lineIndex) {
//     auto &line = array[lineIndex];
//     for (std::vector<unsigned> columns = combinadic.begin(strenth);
//          columns[strenth - 1] < line.size(); combinadic.next(columns)) {
//       for (unsigned i = 0; i < strenth; ++i) {
//         tuple[i] = line[columns[i]];
//       }
//       //			cover(coverage.encode(columns, tuple));
//       unsigned encode = coverage.encode(columns, tuple);
//       coverByLineIndex[encode] = lineIndex;
//       coverage.cover(encode);
//     }
//   }
//   coverage.set_zero_invalid();
//   std::cout << "c actsInitialize: " << double(clock() - start) / CLOCKS_PER_SEC
//             << std::endl;
//   entryTabu.initialize(Entry(array.size(), array.size()));
//   validater.initialize(array);
//   tmpPrint();
//   oneCoveredTuples.initialize(specificationFile, array.size());
//   oneCoveredTuples.pushOneCoveredTuple(coverage, coverByLineIndex);
// #ifndef NDEBUG
//   int i = 0;
//   for (auto &line : array) {
//     std::cout << i++ << ": ";
//     for (auto v : line) {
//       std::cout << v << ' ';
//     }
//     std::cout << std::endl;
//   }
// #endif
// }

void CoveringArray::produceSatRow(std::vector<unsigned> &newLine,
                                  const unsigned encode) {
  const unsigned strength = specificationFile.getStrenth();
  const Options &options = specificationFile.getOptions();
  const unsigned width = options.size();
  assert(width == newLine.size());

  InputKnown known;
  const std::vector<unsigned> &ranTuple = coverage.getTuple(encode);
  const std::vector<unsigned> &ranTupleColumns = coverage.getColumns(encode);
  for (unsigned i = 0; i < strength; ++i) {
    newLine[ranTupleColumns[i]] = ranTuple[i];
    known.append(InputTerm(false, ranTuple[i]));
  }
  std::vector<bool> columnStarted(width, false);
  std::vector<unsigned> columnBases(width);
  for (unsigned column = 0, passing = 0; column < width; ++column) {
    if (passing < strength && column == ranTupleColumns[passing]) {
      passing++;
      continue;
    }
    columnBases[column] = mersenne.next(options.symbolCount(column));
    newLine[column] = options.firstSymbol(column) + columnBases[column] - 1;
  }
  for (long column = 0, passing = 0; column < width; ++column) {
    if (passing < strength && column == ranTupleColumns[passing]) {
      passing++;
      continue;
    }
    const unsigned firstSymbol = options.firstSymbol(column);
    const unsigned lastSymbol = options.lastSymbol(column);
    while (true) {
      newLine[column]++;
      if (newLine[column] > lastSymbol) {
        newLine[column] = firstSymbol;
      }
      if (newLine[column] == firstSymbol + columnBases[column]) {
        // backtrack
        if (columnStarted[column]) {
          columnStarted[column] = false;
          // assign it the value before starting
          newLine[column]--;
          column--;
          while (passing > 0 && column == ranTupleColumns[passing - 1]) {
            column--;
            passing--;
          }
          // undo column++ of the "for" loop
          column--;
          // the var of parent column is now unabled
          known.undoAppend();
          break;
        } else {
          columnStarted[column] = true;
        }
      }
      known.append(InputTerm(false, newLine[column]));
      if (satSolver(known)) {
        break;
      }
      known.undoAppend();
    }
  }
}

void CoveringArray::mostGreedySatRow(const unsigned lineIndex,
                                     const unsigned encode) {
  std::vector<unsigned> &newLine = array[lineIndex];
  const unsigned strength = specificationFile.getStrenth();
  const Options &options = specificationFile.getOptions();
  const unsigned width = options.size();
  assert(width == newLine.size());

  InputKnown known;
  const std::vector<unsigned> &ranTuple = coverage.getTuple(encode);
  const std::vector<unsigned> &ranTupleColumns = coverage.getColumns(encode);
  for (unsigned i = 0; i < strength; ++i) {
    known.append(InputTerm(false, ranTuple[i]));
  }
  cover(encode, lineIndex);

  std::vector<std::set<unsigned>> columnSymbols(width);
  for (unsigned column = 0, passing = 0; column < width; ++column) {
    if (passing < strength && column == ranTupleColumns[passing]) {
      passing++;
      continue;
    }
    for (unsigned symbol = options.firstSymbol(column);
         symbol <= options.lastSymbol(column); ++symbol) {
      known.append(InputTerm(false, symbol));
      if (satSolver(known)) {
        columnSymbols[column].insert(symbol);
      }
      known.undoAppend();
    }
    break;
  }
  std::vector<unsigned> assignment(width);
  for (unsigned i = 0; i < strength; ++i) {
    assignment[ranTupleColumns[i]] = ranTuple[i];
  }
  std::set<unsigned> assignedVar; // it is sorted
  for (auto var : ranTuple) {
    assignedVar.insert(var);
  }
  // note that covering should consider ranTuple
  for (unsigned column = 0, passing = 0; column < width; ++column) {
    if (passing < strength && column == ranTupleColumns[passing]) {
      passing++;
      continue;
    }
    while (true) {
      unsigned maxNewCoverCount = 0;
      std::vector<unsigned> bestVars;
      for (auto var : columnSymbols[column]) {
        // choose best one
        std::vector<unsigned> assignedVarTmp(assignedVar.begin(),
                                             assignedVar.end());
        std::vector<unsigned> tmpTuple(strength);
        std::vector<unsigned> tmpColumns(strength);
        unsigned newCoverCount = 0;
        for (std::vector<unsigned> columns = combinadic.begin(strength - 1);
             columns[strength - 2] < assignedVarTmp.size();
             combinadic.next(columns)) {
          for (unsigned i = 0; i < strength - 1; ++i) {
            tmpTuple[i] = assignedVarTmp[columns[i]];
          }
          tmpTuple[strength - 1] = var;
          std::sort(tmpTuple.begin(), tmpTuple.end());
          for (unsigned i = 0; i < strength; ++i) {
            tmpColumns[i] = options.option(tmpTuple[i]);
          }
          unsigned tmpEncode = coverage.encode(tmpColumns, tmpTuple);
          if (coverage.coverCount(tmpEncode) == 0) {
            newCoverCount++;
          }
        }
        if (newCoverCount > maxNewCoverCount) {
          maxNewCoverCount = newCoverCount;
          bestVars.clear();
          bestVars.push_back(var);
        } else if (newCoverCount == maxNewCoverCount) {
          bestVars.push_back(var);
        }
      }
      if (bestVars.size() == 0) {
        // backtrack, uncover tuples, undoAppend
        column--;
        while (passing > 0 && column == ranTupleColumns[passing - 1]) {
          column--;
          passing--;
        }
        unsigned backtrackVar = assignment[column];
        assignedVar.erase(backtrackVar);
        // uncover tuples
        std::vector<unsigned> tmpTuple(strength);
        std::vector<unsigned> tmpColumns(strength);
        std::vector<unsigned> assignedVarTmp(assignedVar.begin(),
                                             assignedVar.end());
        for (std::vector<unsigned> columns = combinadic.begin(strength - 1);
             columns[strength - 2] < assignedVarTmp.size();
             combinadic.next(columns)) {
          for (unsigned i = 0; i < strength - 1; ++i) {
            tmpTuple[i] = assignedVarTmp[columns[i]];
          }
          tmpTuple[strength - 1] = backtrackVar;
          std::sort(tmpTuple.begin(), tmpTuple.end());
          for (unsigned i = 0; i < strength; ++i) {
            tmpColumns[i] = options.option(tmpTuple[i]);
          }
          uncover(coverage.encode(tmpColumns, tmpTuple), lineIndex);
        }
        // undoAppend
        known.undoAppend();
      } else {
        // break tie randomly
        unsigned ranIndex = mersenne.next(bestVars.size());
        unsigned tmpVar = bestVars[ranIndex];
        known.append(InputTerm(false, tmpVar));
        // cover tuples
        std::vector<unsigned> tmpTuple(strength);
        std::vector<unsigned> tmpColumns(strength);
        std::vector<unsigned> assignedVarTmp(assignedVar.begin(),
                                             assignedVar.end());
        for (std::vector<unsigned> columns = combinadic.begin(strength - 1);
             columns[strength - 2] < assignedVarTmp.size();
             combinadic.next(columns)) {
          for (unsigned i = 0; i < strength - 1; ++i) {
            tmpTuple[i] = assignedVarTmp[columns[i]];
          }
          tmpTuple[strength - 1] = tmpVar;
          std::sort(tmpTuple.begin(), tmpTuple.end());
          for (unsigned i = 0; i < strength; ++i) {
            tmpColumns[i] = options.option(tmpTuple[i]);
          }
          cover(coverage.encode(tmpColumns, tmpTuple), lineIndex);
        }
        assignment[column] = tmpVar;
        assignedVar.insert(tmpVar);
        // close tmpvar
        columnSymbols[column].erase(tmpVar);
        // if it has, initial next column
        column++;
        while (passing < strength && column == ranTupleColumns[passing]) {
          passing++;
          column++;
        }
        column--;
        if (column < width - 1) {
          for (unsigned symbol = options.firstSymbol(column + 1);
               symbol <= options.lastSymbol(column + 1); ++symbol) {
            known.append(InputTerm(false, symbol));
            if (satSolver(known)) {
              columnSymbols[column + 1].insert(symbol);
            }
            known.undoAppend();
          }
        }
        break;
      }
    }
  }
  newLine.assign(assignedVar.begin(), assignedVar.end());
}

void CoveringArray::replaceRow(const unsigned lineIndex,
                               const unsigned encode) {
  std::vector<unsigned> &ranLine = array[lineIndex];
  const unsigned strength = specificationFile.getStrenth();
  std::vector<unsigned> tmpTuple(strength);
  // uncover the tuples
  for (std::vector<unsigned> columns = combinadic.begin(strength);
       columns[strength - 1] < ranLine.size(); combinadic.next(columns)) {
    for (unsigned i = 0; i < strength; ++i) {
      tmpTuple[i] = ranLine[columns[i]];
    }
    uncover(coverage.encode(columns, tmpTuple), lineIndex);
  }
  produceSatRow(ranLine, encode);
  // cover the tuples
  for (std::vector<unsigned> columns = combinadic.begin(strength);
       columns[strength - 1] < ranLine.size(); combinadic.next(columns)) {
    for (unsigned i = 0; i < strength; ++i) {
      tmpTuple[i] = ranLine[columns[i]];
    }
    cover(coverage.encode(columns, tmpTuple), lineIndex);
  }
  entryTabu.initialize(
      Entry(array.size(), specificationFile.getOptions().size()));
}

void CoveringArray::replaceRow2(const unsigned lineIndex,
                                const unsigned encode) {
  std::vector<unsigned> &ranLine = array[lineIndex];
  const unsigned strength = specificationFile.getStrenth();
  std::vector<unsigned> tmpTuple(strength);
  // uncover the tuples
  for (std::vector<unsigned> columns = combinadic.begin(strength);
       columns[strength - 1] < ranLine.size(); combinadic.next(columns)) {
    for (unsigned i = 0; i < strength; ++i) {
      tmpTuple[i] = ranLine[columns[i]];
    }
    uncover(coverage.encode(columns, tmpTuple), lineIndex);
  }

  const std::vector<unsigned> &target_tuple = coverage.getTuple(encode);
  const std::vector<unsigned> &target_columns = coverage.getColumns(encode);
  for (auto &line : bestArray) {
    bool match = true;
    for (size_t i = 0; i < target_columns.size(); ++i) {
      if (line[target_columns[i]] != target_tuple[i]) {
        match = false;
        break;
      }
    }
    if (match) {
      ranLine = line;
      break;
    }
  }

  // cover the tuples
  for (std::vector<unsigned> columns = combinadic.begin(strength);
       columns[strength - 1] < ranLine.size(); combinadic.next(columns)) {
    for (unsigned i = 0; i < strength; ++i) {
      tmpTuple[i] = ranLine[columns[i]];
    }
    cover(coverage.encode(columns, tmpTuple), lineIndex);
  }
  entryTabu.initialize(
      Entry(array.size(), specificationFile.getOptions().size()));
  validater.change_row(lineIndex, ranLine);
}

void CoveringArray::removeUselessRows() {
  const Options &options = specificationFile.getOptions();
  const unsigned strength = specificationFile.getStrenth();
  std::vector<unsigned> tmpTuple(strength);

  for (size_t lineIndex = 0; lineIndex < array.size();) {
    if (oneCoveredTuples.oneCoveredCount(lineIndex) == 0) {
      const std::vector<unsigned> &line = array[lineIndex];
      for (std::vector<unsigned> columns = combinadic.begin(strength);
           columns[strength - 1] < options.size(); combinadic.next(columns)) {
        for (unsigned i = 0; i < strength; ++i) {
          tmpTuple[i] = line[columns[i]];
        }
        unsigned encode = coverage.encode(columns, tmpTuple);
        uncover(encode, lineIndex);
      }
      std::swap(array[lineIndex], array[array.size() - 1]);
      for (auto &entry : entryTabu) {
        if (entry.getRow() == array.size() - 1) {
          entry.setRow(lineIndex);
        }
        if (entry.getRow() == lineIndex) {
          entry.setRow(array.size() - 1);
        }
      }
      oneCoveredTuples.exchange_row(lineIndex, array.size() - 1);
      oneCoveredTuples.pop_back_row();
      array.pop_back();
    } else {
      ++lineIndex;
    }
  }
}

void CoveringArray::removeUselessRows2() {
  const Options &options = specificationFile.getOptions();
  const unsigned strength = specificationFile.getStrenth();
  std::vector<unsigned> tmpTuple(strength);

  for (size_t lineIndex = 0; lineIndex < array.size();) {
    if (oneCoveredTuples.oneCoveredCount(lineIndex) == 0) {
      const std::vector<unsigned> &line = array[lineIndex];
      for (std::vector<unsigned> columns = combinadic.begin(strength);
           columns[strength - 1] < options.size(); combinadic.next(columns)) {
        for (unsigned i = 0; i < strength; ++i) {
          tmpTuple[i] = line[columns[i]];
        }
        unsigned encode = coverage.encode(columns, tmpTuple);
        uncover(encode, lineIndex);
      }
      std::swap(array[lineIndex], array[array.size() - 1]);
      for (auto &entry : entryTabu) {
        if (entry.getRow() == array.size() - 1) {
          entry.setRow(lineIndex);
        }
        if (entry.getRow() == lineIndex) {
          entry.setRow(array.size() - 1);
        }
      }
      validater.exchange_row(lineIndex, array.size() - 1);
      validater.pop_back_row();
      oneCoveredTuples.exchange_row(lineIndex, array.size() - 1);
      oneCoveredTuples.pop_back_row();
      array.pop_back();
    } else {
      ++lineIndex;
    }
  }
}

void CoveringArray::removeOneRow() {
  const Options &options = specificationFile.getOptions();
  const unsigned strength = specificationFile.getStrenth();

  std::vector<unsigned> bestRowIndex;
  bestRowIndex.push_back(0);
  unsigned minOneCoveredCount = oneCoveredTuples.oneCoveredCount(0);
  for (size_t lineIndex = 1; lineIndex < array.size(); ++lineIndex) {
    unsigned oneCoveredCount = oneCoveredTuples.oneCoveredCount(lineIndex);
    if (minOneCoveredCount > oneCoveredCount) {
      bestRowIndex.clear();
      bestRowIndex.push_back(lineIndex);
      minOneCoveredCount = oneCoveredCount;
    } else if (minOneCoveredCount == oneCoveredCount) {
      bestRowIndex.push_back(lineIndex);
    }
  }

  unsigned rowToremoveIndex = bestRowIndex[mersenne.next(bestRowIndex.size())];
  std::vector<unsigned> tmpTuple(strength);
  for (std::vector<unsigned> columns = combinadic.begin(strength);
       columns[strength - 1] < options.size(); combinadic.next(columns)) {
    for (unsigned j = 0; j < strength; ++j) {
      tmpTuple[j] = array[rowToremoveIndex][columns[j]];
    }
    unsigned encode = coverage.encode(columns, tmpTuple);
    uncover(encode, rowToremoveIndex);
  }

  std::swap(array[array.size() - 1], array[rowToremoveIndex]);
  validater.exchange_row(rowToremoveIndex, array.size() - 1);
  validater.pop_back_row();
  oneCoveredTuples.exchange_row(rowToremoveIndex, array.size() - 1);
  oneCoveredTuples.pop_back_row();
  for (auto &entry : entryTabu) {
    if (entry.getRow() == array.size() - 1) {
      entry.setRow(rowToremoveIndex);
    }
    if (entry.getRow() == rowToremoveIndex) {
      entry.setRow(array.size() - 1);
    }
  }
  array.pop_back();
}

void CoveringArray::removeOneRowRandom() {
  const Options &options = specificationFile.getOptions();
  const unsigned strength = specificationFile.getStrenth();

  unsigned rowToremoveIndex = mersenne.next(array.size());
  std::vector<unsigned> tmpTuple(strength);
  for (std::vector<unsigned> columns = combinadic.begin(strength);
       columns[strength - 1] < options.size(); combinadic.next(columns)) {
    for (unsigned j = 0; j < strength; ++j) {
      tmpTuple[j] = array[rowToremoveIndex][columns[j]];
    }
    unsigned encode = coverage.encode(columns, tmpTuple);
    uncover(encode, rowToremoveIndex);
  }

  std::swap(array[array.size() - 1], array[rowToremoveIndex]);
  validater.exchange_row(rowToremoveIndex, array.size() - 1);
  validater.pop_back_row();
  oneCoveredTuples.exchange_row(rowToremoveIndex, array.size() - 1);
  oneCoveredTuples.pop_back_row();
  for (auto &entry : entryTabu) {
    if (entry.getRow() == array.size() - 1) {
      entry.setRow(rowToremoveIndex);
    }
    if (entry.getRow() == rowToremoveIndex) {
      entry.setRow(array.size() - 1);
    }
  }
  array.pop_back();
}


void CoveringArray::optimize() {
  clock_t sub_clock_start;

  for (auto &cf : configurationFile) {
    std::string algoName(cf[0]);
    sscanf(cf[1].c_str(), "%llu", &maxTime);
    sscanf(cf[2].c_str(), "%d", &checkFunction);
    sscanf(cf[3].c_str(), "%d", &scoringFunction);
    sscanf(cf[4].c_str(), "%d", &gradientDecent);
    sscanf(cf[5].c_str(), "%ld", &tabuSize);
    sscanf(cf[6].c_str(), "%ld", &modeBalance);

    entryTabu.initialize(
        tabuSize, Entry(array.size(), specificationFile.getOptions().size()));

    sub_clock_start = clock();
    while (true) {
      if ((double)(clock() - sub_clock_start) / CLOCKS_PER_SEC > maxTime) {
        break;
      }
      if (uncoveredTuples.size() == 0) {
        removeUselessRows2();
        bestArray = array;
        tmpPrint();
        //removeOneRow();
        removeOneRowRandom();
      }

      tabuStepHybridGD();
    

      step++;
      continue;
    }
    //std::cout<< "c " << algoName << " is DONE!" << std::endl;
  }
  

  if (uncoveredTuples.size() == 0) {
    removeUselessRows2();
    bestArray = array;
    tmpPrint();
  }
  if (finalCheck == 1){
    if (!verify(bestArray)) {
      std::cout << "c wrong answer!!!!!" << std::endl;
      return;
    }
    else {
      std::cout<< "c correct answer!!!" << std::endl;
    }
  }
  else {
    std::cout<< "c correct answer!!!" << std::endl;
  }

  std::cout << "c total steps: " << step << std::endl;
  std::cout << "c total size : " << bestArray.size() << std::endl;

#ifndef NDEBUG
  std::cerr << "********Debuging CoveringArray::optimize*********" << std::endl;
  std::cerr << "printing bestArray..." << std::endl;
  for (unsigned i = 0; i < bestArray.size(); ++i) {
    std::cerr << i << "th  ";
    for (auto x : bestArray[i]) {
      std::cerr << ' ' << x;
    }
    std::cerr << std::endl;
  }
  std::cerr << "total size : " << bestArray.size() << std::endl;
  std::cerr << "********End of Debuing CoveringArray::optimize********"
            << std::endl;
#endif
}

void CoveringArray::tabuStep() {
  const unsigned tupleEncode =
      uncoveredTuples.encode(mersenne.next(uncoveredTuples.size()));
  const std::vector<unsigned> &tuple = coverage.getTuple(tupleEncode);
  const std::vector<unsigned> &columns = coverage.getColumns(tupleEncode);
  if (mersenne.next(1000) < 1) {
    replaceRow(mersenne.next(array.size()), tupleEncode);
    return;
  }
  std::vector<unsigned> bestRows;
  std::vector<unsigned> bestVars;
  long long bestScore = std::numeric_limits<long long>::min();
  for (unsigned lineIndex = 0; lineIndex < array.size(); ++lineIndex) {
    std::vector<unsigned> &line = array[lineIndex];
    unsigned diffCount = 0;
    unsigned diffVar;
    for (unsigned i = 0; i < tuple.size(); ++i) {
      if (line[columns[i]] != tuple[i]) {
        diffCount++;
        diffVar = tuple[i];
      }
    }
    if (diffCount > 1) {
      continue;
    }
    unsigned diffOption = specificationFile.getOptions().option(diffVar);
    // Tabu
    if (entryTabu.isTabu(Entry(lineIndex, diffOption))) {
      continue;
    }
    // check if the new assignment will follow the constraints
    InputKnown known;
    for (unsigned i = 0; i < line.size(); ++i) {
      if (i == diffOption) {
        known.append(InputTerm(false, diffVar));
      } else {
        known.append(InputTerm(false, line[i]));
      }
    }
    if (!satSolver(known)) {
      continue;
    }
    long long tmpScore = varScoreOfRow3(diffVar, lineIndex);
    if (bestScore < tmpScore) {
      bestScore = tmpScore;
      bestRows.clear();
      bestRows.push_back(lineIndex);
      bestVars.clear();
      bestVars.push_back(diffVar);
    } else if (bestScore == tmpScore) {
      bestRows.push_back(lineIndex);
      bestVars.push_back(diffVar);
    }
  }
  if (bestRows.size() != 0) {
    unsigned ran = mersenne.next(bestRows.size());
    replace(bestVars[ran], bestRows[ran]);
    return;
  }

  if (mersenne.next(100) < 1) {
    replaceRow(mersenne.next(array.size()), tupleEncode);
    return;
  }

  std::vector<unsigned> changedVars;
  for (unsigned lineIndex = 0; lineIndex < array.size(); ++lineIndex) {
    changedVars.clear();
    std::vector<unsigned> &line = array[lineIndex];
    for (unsigned i = 0; i < tuple.size(); ++i) {
      if (line[columns[i]] != tuple[i]) {
        changedVars.push_back(tuple[i]);
      }
    }
    if (changedVars.size() == 0) {
      continue;
    }
    // check constraint, before tmpScore or after it?
    InputKnown known;
    for (unsigned column = 0, passing = 0; column < line.size(); ++column) {
      if (passing < tuple.size() && column == columns[passing]) {
        known.append(InputTerm(false, tuple[passing++]));
      } else {
        known.append(InputTerm(false, line[column]));
      }
    }
    if (!satSolver(known)) {
      continue;
    }
    // greedy
    long long tmpScore = multiVarScoreOfRow(changedVars, lineIndex);
    if (bestScore < tmpScore) {
      bestScore = tmpScore;
      bestRows.clear();
      bestRows.push_back(lineIndex);
    } else if (bestScore == tmpScore) {
      bestRows.push_back(lineIndex);
    }
  }
  // need to handle when bestRows.size() == 0
  if (bestRows.size() != 0) {
    unsigned lineIndex = bestRows[mersenne.next(bestRows.size())];
    changedVars.clear();
    for (unsigned i = 0; i < tuple.size(); ++i) {
      if (array[lineIndex][columns[i]] != tuple[i]) {
        changedVars.push_back(tuple[i]);
      }
    }
    multiVarReplace(changedVars, lineIndex);
    return;
  }
  replaceRow(mersenne.next(array.size()), tupleEncode);
}

long long
CoveringArray::multiVarRow(const std::vector<unsigned> &sortedMultiVars,
                           const unsigned lineIndex, const bool change) {
  const Options &options = specificationFile.getOptions();
  const unsigned strength = specificationFile.getStrenth();
  long long score = 0;

  std::vector<unsigned> varColumns;
  for (auto var : sortedMultiVars) {
    varColumns.push_back(options.option(var));
  }
  if (change) {
    for (auto column : varColumns) {
      entryTabu.insert(Entry(lineIndex, column));
    }
  }
  std::vector<unsigned> &line = array[lineIndex];

  // must from the end to the begining
  for (unsigned i = sortedMultiVars.size(); i--;) {
    std::swap(line[line.size() - sortedMultiVars.size() + i],
              line[varColumns[i]]);
  }

  std::vector<unsigned> tmpSortedColumns(strength);
  std::vector<unsigned> tmpSortedTupleToCover(strength);
  std::vector<unsigned> tmpSortedTupleToUncover(strength);
  unsigned tmpToCoverEncode;
  unsigned tmpToUncoverEncode;

  if (sortedMultiVars.size() >= strength) {
    for (std::vector<unsigned> changedColums = combinadic.begin(strength);
         changedColums[strength - 1] < sortedMultiVars.size();
         combinadic.next(changedColums)) {
      for (unsigned i = 0; i < strength; ++i) {
        tmpSortedTupleToCover[i] = sortedMultiVars[changedColums[i]];
        tmpSortedTupleToUncover[i] =
            line[line.size() - sortedMultiVars.size() + changedColums[i]];
      }
      std::sort(tmpSortedTupleToCover.begin(), tmpSortedTupleToCover.end());
      std::sort(tmpSortedTupleToUncover.begin(), tmpSortedTupleToUncover.end());
      for (unsigned i = 0; i < strength; ++i) {
        tmpSortedColumns[i] = options.option(tmpSortedTupleToCover[i]);
      }
      tmpToCoverEncode =
          coverage.encode(tmpSortedColumns, tmpSortedTupleToCover);
      tmpToUncoverEncode =
          coverage.encode(tmpSortedColumns, tmpSortedTupleToUncover);
      if (change) {
        cover(tmpToCoverEncode, lineIndex);
        uncover(tmpToUncoverEncode, lineIndex);
      } else {
        if (coverage.coverCount(tmpToCoverEncode) == 0) {
          ++score;
        }
        if (coverage.coverCount(tmpToUncoverEncode) == 1) {
          --score;
        }
      }
    }
  }

  for (unsigned curRelevantCount = 1,
                maxRelevantCount = std::min(
                    strength - 1,
                    (const unsigned)(line.size() - sortedMultiVars.size()));
       curRelevantCount <= maxRelevantCount; ++curRelevantCount) {
    for (std::vector<unsigned> relevantColumns =
             combinadic.begin(curRelevantCount);
         relevantColumns[curRelevantCount - 1] <
         line.size() - sortedMultiVars.size();
         combinadic.next(relevantColumns)) {
      for (std::vector<unsigned> changedColums =
               combinadic.begin(strength - curRelevantCount);
           changedColums[strength - curRelevantCount - 1] <
           sortedMultiVars.size();
           combinadic.next(changedColums)) {

        for (unsigned i = 0; i < curRelevantCount; ++i) {
          tmpSortedTupleToCover[i] = tmpSortedTupleToUncover[i] =
              line[relevantColumns[i]];
        }
        for (unsigned i = 0; i < strength - curRelevantCount; ++i) {
          tmpSortedTupleToCover[curRelevantCount + i] =
              sortedMultiVars[changedColums[i]];
          tmpSortedTupleToUncover[curRelevantCount + i] =
              line[line.size() - sortedMultiVars.size() + changedColums[i]];
        }
        std::sort(tmpSortedTupleToCover.begin(), tmpSortedTupleToCover.end());
        std::sort(tmpSortedTupleToUncover.begin(),
                  tmpSortedTupleToUncover.end());
        for (unsigned i = 0; i < strength; ++i) {
          tmpSortedColumns[i] = options.option(tmpSortedTupleToCover[i]);
        }
        tmpToCoverEncode =
            coverage.encode(tmpSortedColumns, tmpSortedTupleToCover);
        tmpToUncoverEncode =
            coverage.encode(tmpSortedColumns, tmpSortedTupleToUncover);
        if (change) {
          cover(tmpToCoverEncode, lineIndex);
          uncover(tmpToUncoverEncode, lineIndex);
        } else {
          if (coverage.coverCount(tmpToCoverEncode) == 0) {
            ++score;
          }
          if (coverage.coverCount(tmpToUncoverEncode) == 1) {
            --score;
          }
        }
      }
    }
  }
  // must from the begining to the end
  for (unsigned i = 0; i < sortedMultiVars.size(); ++i) {
    std::swap(line[line.size() - sortedMultiVars.size() + i],
              line[varColumns[i]]);
  }

  if (change) {
    for (unsigned i = 0; i < sortedMultiVars.size(); ++i) {
      line[varColumns[i]] = sortedMultiVars[i];
    }
  }
  return score;
}

long long
CoveringArray::multiVarScoreOfRow(const std::vector<unsigned> &sortedMultiVars,
                                  const unsigned lineIndex) {
  return multiVarRow(sortedMultiVars, lineIndex, false);
}
void CoveringArray::multiVarReplace(
    const std::vector<unsigned> &sortedMultiVars, const unsigned lineIndex) {
  multiVarRow(sortedMultiVars, lineIndex, true);
}

void CoveringArray::multiVarReplace2(
    const std::vector<unsigned> &sortedMultiVars, const unsigned lineIndex) {
  const Options &options = specificationFile.getOptions();
  std::vector<unsigned> org_vars;
  for (auto var : sortedMultiVars) {
    auto option = options.option(var);
    org_vars.push_back(array[lineIndex][option]);
  }
  validater.change_mutivar(lineIndex, org_vars, sortedMultiVars);
  multiVarRow(sortedMultiVars, lineIndex, true);
}

long long CoveringArray::varScoreOfRow3(const unsigned var,
                                        const unsigned lineIndex) {
  const Options &options = specificationFile.getOptions();
  std::vector<unsigned> &line = array[lineIndex];
  const unsigned varOption = options.option(var);
  if (line[varOption] == var) {
    return 0;
  }
  long long coverChangeCount = 0;
  for (auto tupleEncode : uncoveredTuples) {
    const std::vector<unsigned> &tuple = coverage.getTuple(tupleEncode);
    const std::vector<unsigned> &columns = coverage.getColumns(tupleEncode);
    bool match = false;
    bool needChange = true;
    for (size_t i = 0; i < columns.size(); ++i) {
      if (columns[i] == varOption) {
        match = true;
        if (tuple[i] != var) {
          needChange = false;
          break;
        }
      } else if (line[columns[i]] != tuple[i]) {
        needChange = false;
        break;
      }
    }
    if (match && needChange) {
      coverChangeCount++;
    }
  }
  return coverChangeCount -
         oneCoveredTuples.getECbyLineVar(lineIndex, line[varOption]).size();
  for (auto &ecEntry :
       oneCoveredTuples.getECbyLineVar(lineIndex, line[varOption])) {
    unsigned tupleEncode = ecEntry.encode;
    const std::vector<unsigned> &tuple = coverage.getTuple(tupleEncode);
    const std::vector<unsigned> &columns = coverage.getColumns(tupleEncode);
    bool needChange = true;
    for (size_t i = 0; i < columns.size(); ++i) {
      if (line[columns[i]] != tuple[i]) {
        needChange = false;
        break;
      }
    }
    if (needChange) {
      coverChangeCount--;
    }
  }
  return coverChangeCount;
}

long long CoveringArray::varScoreOfRow(const unsigned var,
                                       const unsigned lineIndex) {
  const Options &options = specificationFile.getOptions();
  const unsigned strength = specificationFile.getStrenth();
  std::vector<unsigned> &line = array[lineIndex];
  const unsigned varOption = options.option(var);
  if (line[varOption] == var) {
    return 0;
  }
  std::swap(line[line.size() - 1], line[varOption]);

  long long coverChangeCount = 0;
  std::vector<unsigned> tmpSortedColumns(strength);
  std::vector<unsigned> tmpSortedTupleToCover(strength);
  std::vector<unsigned> tmpSortedTupleToUncover(strength);
  for (std::vector<unsigned> columns = combinadic.begin(strength - 1);
       columns[strength - 2] < line.size() - 1; combinadic.next(columns)) {
    for (unsigned i = 0; i < columns.size(); ++i) {
      tmpSortedTupleToUncover[i] = tmpSortedTupleToCover[i] = line[columns[i]];
    }
    tmpSortedTupleToCover[strength - 1] = var;
    tmpSortedTupleToUncover[strength - 1] = line[line.size() - 1];
    std::sort(tmpSortedTupleToCover.begin(), tmpSortedTupleToCover.end());
    std::sort(tmpSortedTupleToUncover.begin(), tmpSortedTupleToUncover.end());
    for (unsigned i = 0; i < tmpSortedTupleToCover.size(); ++i) {
      tmpSortedColumns[i] = options.option(tmpSortedTupleToCover[i]);
    }
    unsigned tmpTupleToCoverEncode =
        coverage.encode(tmpSortedColumns, tmpSortedTupleToCover);
    unsigned tmpTupleToUncoverEncode =
        coverage.encode(tmpSortedColumns, tmpSortedTupleToUncover);
    if (coverage.coverCount(tmpTupleToCoverEncode) == 0) {
      coverChangeCount++;
    }
    if (coverage.coverCount(tmpTupleToUncoverEncode) == 1) {
      coverChangeCount--;
    }
  }

  std::swap(line[line.size() - 1], line[varOption]);

  return coverChangeCount;
}

// quite similar to varScoreOfRow function
void CoveringArray::replace(const unsigned var, const unsigned lineIndex) {
  const Options &options = specificationFile.getOptions();
  const unsigned strength = specificationFile.getStrenth();
  std::vector<unsigned> &line = array[lineIndex];
  const unsigned varOption = options.option(var);

  entryTabu.insert(Entry(lineIndex, varOption));

  if (line[varOption] == var) {
    return;
  }
  validater.change_var(lineIndex, varOption, line[varOption], var);
  std::swap(line[line.size() - 1], line[varOption]);

  std::vector<unsigned> tmpSortedColumns(strength);
  std::vector<unsigned> tmpSortedTupleToCover(strength);
  std::vector<unsigned> tmpSortedTupleToUncover(strength);
  for (std::vector<unsigned> columns = combinadic.begin(strength - 1);
       columns[strength - 2] < line.size() - 1; combinadic.next(columns)) {
    for (unsigned i = 0; i < columns.size(); ++i) {
      tmpSortedTupleToUncover[i] = tmpSortedTupleToCover[i] = line[columns[i]];
    }
    tmpSortedTupleToCover[strength - 1] = var;
    tmpSortedTupleToUncover[strength - 1] = line[line.size() - 1];
    std::sort(tmpSortedTupleToCover.begin(), tmpSortedTupleToCover.end());
    std::sort(tmpSortedTupleToUncover.begin(), tmpSortedTupleToUncover.end());
    for (unsigned i = 0; i < tmpSortedTupleToCover.size(); ++i) {
      tmpSortedColumns[i] = options.option(tmpSortedTupleToCover[i]);
    }
    unsigned tmpTupleToCoverEncode =
        coverage.encode(tmpSortedColumns, tmpSortedTupleToCover);
    unsigned tmpTupleToUncoverEncode =
        coverage.encode(tmpSortedColumns, tmpSortedTupleToUncover);
    // need not check coverCount, cover(encode) will do this
    cover(tmpTupleToCoverEncode, lineIndex);
    uncover(tmpTupleToUncoverEncode, lineIndex);
  }
  std::swap(line[line.size() - 1], line[varOption]);
  line[varOption] = var;
}

void CoveringArray::cover(const unsigned encode, const unsigned oldLineIndex) {
  coverage.cover(encode);
  unsigned coverCount = coverage.coverCount(encode);
  if (coverCount == 1) {
    uncoveredTuples.pop(encode);
    oneCoveredTuples.push(encode, oldLineIndex, coverage.getTuple(encode));
  }
  if (coverCount == 2) {
    const std::vector<unsigned> &tuple = coverage.getTuple(encode);
    const std::vector<unsigned> &columns = coverage.getColumns(encode);
    for (size_t lineIndex = 0; lineIndex < array.size(); ++lineIndex) {
      if (lineIndex == oldLineIndex) {
        continue;
      }
      auto &line = array[lineIndex];
      bool match = true;
      for (size_t i = 0; i < columns.size(); ++i) {
        if (tuple[i] != line[columns[i]]) {
          match = false;
          break;
        }
      }
      if (match) {
        oneCoveredTuples.pop(encode, lineIndex, tuple);
        break;
      }
    }
  }
}

void CoveringArray::uncover(const unsigned encode,
                            const unsigned oldLineIndex) {
  coverage.uncover(encode);
  unsigned coverCount = coverage.coverCount(encode);
  if (coverCount == 0) {
    uncoveredTuples.push(encode);
    oneCoveredTuples.pop(encode, oldLineIndex, coverage.getTuple(encode));
  }
  if (coverCount == 1) {
    const std::vector<unsigned> &tuple = coverage.getTuple(encode);
    const std::vector<unsigned> &columns = coverage.getColumns(encode);
    for (size_t lineIndex = 0; lineIndex < array.size(); ++lineIndex) {
      if (lineIndex == oldLineIndex) {
        continue;
      }
      auto &line = array[lineIndex];
      bool match = true;
      for (size_t i = 0; i < columns.size(); ++i) {
        if (tuple[i] != line[columns[i]]) {
          match = false;
          break;
        }
      }
      if (match) {
        oneCoveredTuples.push(encode, lineIndex, tuple);
        break;
      }
    }
  }
}

void CoveringArray::tmpPrint() {
  std::cout << (double)(clock() - clock_start) / CLOCKS_PER_SEC << '\t'
            << array.size() << '\t' << step << std::endl;
  
  FILE *fout = fopen("../___AutoCCAG_temp_CA_file", "w");
  for (unsigned i = 0; i < bestArray.size(); ++i) {
    int sz = (int) bestArray[i].size();
    for (int j = 0; j < sz; ++j)
      fprintf(fout, "%u%c", bestArray[i][j], " \n"[j == sz - 1]);
  }
  fclose(fout);
}

bool CoveringArray::verify(
    const std::vector<std::vector<unsigned>> &resultArray) {
  const unsigned strength = specificationFile.getStrenth();
  const Options &options = specificationFile.getOptions();
  Coverage tmpCoverage(specificationFile);
  tmpCoverage.initialize(satSolver, option_constrained_indicator);
  std::vector<unsigned> tuple(strength);
  unsigned lineIndex = 0;
  for (auto &line : resultArray) {
    for (unsigned column = 0; column < line.size(); ++column) {
      if (line[column] < options.firstSymbol(column) ||
          line[column] > options.lastSymbol(column)) {
        std::cerr << "error line: " << lineIndex;
        std::cerr << " option: " << column << std::endl;
        std::cerr << "should be " << options.firstSymbol(column)
                  << " <= var <= " << options.lastSymbol(column) << std::endl;
        for (auto var : line) {
          std::cerr << var << ' ';
        }
        std::cerr << std::endl;
        return false;
      }
    }
    for (std::vector<unsigned> columns = combinadic.begin(strength);
         columns[strength - 1] < line.size(); combinadic.next(columns)) {
      for (unsigned i = 0; i < strength; ++i) {
        tuple[i] = line[columns[i]];
      }
      unsigned encode = tmpCoverage.encode(columns, tuple);
      if (tmpCoverage.coverCount(encode) < 0) {
        std::cerr << "violate constraints" << std::endl;
        return false;
      }
      tmpCoverage.cover(encode);
    }
    ++lineIndex;
  }
  return tmpCoverage.allIsCovered();
}

void CoveringArray::tabuStepHybrid() {
  const unsigned tupleEncode =
      uncoveredTuples.encode(mersenne.next(uncoveredTuples.size()));
  const std::vector<unsigned> &tuple = coverage.getTuple(tupleEncode);
  const std::vector<unsigned> &columns = coverage.getColumns(tupleEncode);
  if (mersenne.next(1000) < 1) {
    replaceRow2(mersenne.next(array.size()), tupleEncode);
    return;
  }
  std::vector<unsigned> bestRows;
  std::vector<unsigned> bestVars;
  long long bestScore = std::numeric_limits<long long>::min();
  for (unsigned lineIndex = 0; lineIndex < array.size(); ++lineIndex) {
    std::vector<unsigned> &line = array[lineIndex];
    unsigned diffCount = 0;
    unsigned diffVar;
    for (unsigned i = 0; i < tuple.size(); ++i) {
      if (line[columns[i]] != tuple[i]) {
        diffCount++;
        diffVar = tuple[i];
      }
    }
    if (diffCount > 1) {
      continue;
    }
    unsigned diffOption = specificationFile.getOptions().option(diffVar);
    // Tabu
    if (entryTabu.isTabu(Entry(lineIndex, diffOption))) {
      continue;
    }
    // check if the new assignment will follow the constraints
    if (checkFunction == 1) {
      if (!validater.valida_change(lineIndex, diffOption, line[diffOption],
                                   diffVar)) {
        continue;
      }
    }
    else {
      InputKnown known;
      for (unsigned column = 0, passing = 0; column < line.size(); ++column) {
        if (passing < tuple.size() && column == columns[passing]) {
          known.append(InputTerm(false, tuple[passing++]));
        } else {
          known.append(InputTerm(false, line[column]));
        }
      }
      if (!satSolver(known)) {
        continue;
      }
    }
    long long tmpScore = varScoreOfRow3(diffVar, lineIndex);
    if (bestScore < tmpScore) {
      bestScore = tmpScore;
      bestRows.clear();
      bestRows.push_back(lineIndex);
      bestVars.clear();
      bestVars.push_back(diffVar);
    } else if (bestScore == tmpScore) {
      bestRows.push_back(lineIndex);
      bestVars.push_back(diffVar);
    }
  }
  if (bestRows.size() != 0) {
    unsigned ran = mersenne.next(bestRows.size());
    replace(bestVars[ran], bestRows[ran]);
    return;
  }

  if (mersenne.next(1000) < 1) {
    replaceRow2(mersenne.next(array.size()), tupleEncode);
    return;
  }
  std::vector<unsigned> changedVars;
  for (unsigned lineIndex = 0; lineIndex < array.size(); ++lineIndex) {
    changedVars.clear();
    std::vector<unsigned> &line = array[lineIndex];
    for (unsigned i = 0; i < tuple.size(); ++i) {
      if (line[columns[i]] != tuple[i]) {
        changedVars.push_back(tuple[i]);
      }
    }
    if (changedVars.size() == 0) {
      continue;
    }
    // check constraint, before tmpScore or after it?
    if (checkFunction == 1) {
      bool need_to_check = false;
      for (auto v : changedVars) {
        const Options &options = specificationFile.getOptions();
        if (option_constrained_indicator[options.option(v)]) {
          need_to_check = true;
          break;
        }
      }
      if (need_to_check && !validater.valida_row(array[lineIndex], changedVars)) {
        continue;
      }
    }
    else {
      InputKnown known;
      for (unsigned column = 0, passing = 0; column < line.size(); ++column) {
        if (passing < tuple.size() && column == columns[passing]) {
          known.append(InputTerm(false, tuple[passing++]));
        } else {
          known.append(InputTerm(false, line[column]));
        }
      }
      if (!satSolver(known)) {
        continue;
      }
    }
    // greedy
    long long tmpScore;
    if (scoringFunction == 1){
      if (changedVars.size() > 1) {
        tmpScore = multiVarScoreOfRow2(changedVars, lineIndex);
      } else {
        tmpScore = varScoreOfRow3(changedVars[0], lineIndex);
      }
    }
    else {  
      tmpScore = multiVarScoreOfRow(changedVars, lineIndex);
    }
    if (bestScore < tmpScore) {
      bestScore = tmpScore;
      bestRows.clear();
      bestRows.push_back(lineIndex);
    } else if (bestScore == tmpScore) {
      bestRows.push_back(lineIndex);
    }
  }
  // need to handle when bestRows.size() == 0
  if (bestRows.size() != 0) {
    unsigned lineIndex = bestRows[mersenne.next(bestRows.size())];
    changedVars.clear();
    for (unsigned i = 0; i < tuple.size(); ++i) {
      if (array[lineIndex][columns[i]] != tuple[i]) {
        changedVars.push_back(tuple[i]);
      }
    }
    multiVarReplace2(changedVars, lineIndex);
    return;
  }
  replaceRow2(mersenne.next(array.size()), tupleEncode);
}

long long
CoveringArray::multiVarScoreOfRow2(const std::vector<unsigned> &sortedMultiVars,
                                   const unsigned lineIndex) {
  const Options &options = specificationFile.getOptions();
  std::vector<unsigned> &line = array[lineIndex];
  std::vector<unsigned> changedColumns;
  for (auto var : sortedMultiVars) {
    int c = options.option(var);
    changedColumns.push_back(c);
  }

  long long coverChangeCount = 0;
  for (auto tupleEncode : uncoveredTuples) {
    const std::vector<unsigned> &tuple = coverage.getTuple(tupleEncode);
    const std::vector<unsigned> &columns = coverage.getColumns(tupleEncode);

    bool needChange = true;
    size_t i = 0, j = 0;
    while (i != columns.size() && j != changedColumns.size()) {
      if (columns[i] == changedColumns[j]) {
        if (tuple[i] != sortedMultiVars[j]) {
          needChange = false;
          break;
        }
        i++;
        j++;
      } else {
        if (columns[i] < changedColumns[j]) {
          if (tuple[i] != line[columns[i]]) {
            needChange = false;
            break;
          }
          i++;
        } else {
          j++;
        }
      }
    }
    for (; i != columns.size(); ++i) {
      if (tuple[i] != line[columns[i]]) {
        needChange = false;
        break;
      }
    }
    if (needChange) {
      coverChangeCount++;
    }
  }
  std::vector<long long> overlapCount(changedColumns.size() + 1, 0);
  for (auto cc : changedColumns) {
    for (auto &ecEntry : oneCoveredTuples.getECbyLineVar(lineIndex, line[cc])) {
      unsigned tupleEncode = ecEntry.encode;
      const std::vector<unsigned> &tuple = coverage.getTuple(tupleEncode);
      const std::vector<unsigned> &columns = coverage.getColumns(tupleEncode);
      bool needChange = true;
      for (size_t i = 0; i < columns.size(); ++i) {
        if (line[columns[i]] != tuple[i]) {
          needChange = false;
          break;
        }
      }
      if (needChange) {
        long long olc = 0;
        size_t j = 0, k = 0;
        while (j != columns.size() && k != changedColumns.size()) {
          if (columns[j] == changedColumns[k]) {
            olc++;
            j++;
            k++;
          } else {
            columns[j] < changedColumns[k] ? j++ : k++;
          }
        }
        overlapCount[olc]++;
      }
    }
  }
  for (size_t olc = 1; olc < overlapCount.size(); ++olc) {
    coverChangeCount -= overlapCount[olc] / olc;
  }
  return coverChangeCount;
}


void CoveringArray::tabuStepHybridGD() {
  unsigned base = mersenne.next(uncoveredTuples.size());
  std::vector<unsigned> firstBestRows;
  std::vector<unsigned> firstBestVars;
  std::vector<unsigned> bestRows;
  std::vector<unsigned> bestVars;
  long long bestScore = std::numeric_limits<long long>::min();

  if (specialRandomOn == 1 && mersenne.next(1000) < 1) {
    const unsigned tupleEncode = uncoveredTuples.encode(base);
    replaceRow2(mersenne.next(array.size()), tupleEncode);
    return;
  }

  for (size_t i = 0; i < uncoveredTuples.size(); ++i) {
    const unsigned tupleEncode =
        uncoveredTuples.encode((base + i) % uncoveredTuples.size());
    const std::vector<unsigned> &tuple = coverage.getTuple(tupleEncode);
    const std::vector<unsigned> &columns = coverage.getColumns(tupleEncode);
    for (unsigned lineIndex = 0; lineIndex < array.size(); ++lineIndex) {
      std::vector<unsigned> &line = array[lineIndex];
      unsigned diffCount = 0;
      unsigned diffVar;
      for (unsigned i = 0; i < tuple.size(); ++i) {
        if (line[columns[i]] != tuple[i]) {
          diffCount++;
          diffVar = tuple[i];
        }
      }
      if (diffCount > 1) {
        continue;
      }
      unsigned diffOption = specificationFile.getOptions().option(diffVar);
      // Tabu
      if (entryTabu.isTabu(Entry(lineIndex, diffOption))) {
        continue;
      }
      // my check
      if (checkFunction == 1) {
        if (!validater.valida_change(lineIndex, diffOption, line[diffOption],
                                     diffVar)) {
          continue;
        }
      }
      else {
        InputKnown known;
        for (unsigned column = 0, passing = 0; column < line.size(); ++column) {
          if (passing < tuple.size() && column == columns[passing]) {
            known.append(InputTerm(false, tuple[passing++]));
          } else {
            known.append(InputTerm(false, line[column]));
          }
        }
        if (!satSolver(known)) {
          continue;
        }
      }
      long long tmpScore = varScoreOfRow3(diffVar, lineIndex);
      if (bestScore < tmpScore) {
        bestScore = tmpScore;
        if (i == 0) {
          firstBestRows.clear();
          firstBestRows.push_back(lineIndex);
          firstBestVars.clear();
          firstBestVars.push_back(diffVar);
        }
        bestRows.clear();
        bestRows.push_back(lineIndex);
        bestVars.clear();
        bestVars.push_back(diffVar);
      } else if (bestScore == tmpScore) {
        bestRows.push_back(lineIndex);
        bestVars.push_back(diffVar);
        if (i == 0) {
          firstBestRows.push_back(lineIndex);
          firstBestVars.push_back(diffVar);
        }
      }
    }
    if (gradientDecent != 1) {
      break;
    }
  }

  if (gradientDecent == 1 && bestScore > 0) {
    unsigned ran = mersenne.next(bestRows.size());
    replace(bestVars[ran], bestRows[ran]);
    return;
  }

  if (modeBalance >= 0 && mersenne.next(modeBalance) < 1) {
    replaceRow2(mersenne.next(array.size()), uncoveredTuples.encode(base));
    return;
  }

  if (firstBestRows.size() != 0) {
    unsigned ran = mersenne.next(firstBestRows.size());
    replace(firstBestVars[ran], firstBestRows[ran]);
    return;
  }

  const unsigned tupleEncode =
      uncoveredTuples.encode(base % uncoveredTuples.size());
  const std::vector<unsigned> &tuple = coverage.getTuple(tupleEncode);
  const std::vector<unsigned> &columns = coverage.getColumns(tupleEncode);

  bestRows.clear();

  std::vector<unsigned> changedVars;
  for (unsigned lineIndex = 0; lineIndex < array.size(); ++lineIndex) {
    changedVars.clear();
    std::vector<unsigned> &line = array[lineIndex];
    for (unsigned i = 0; i < tuple.size(); ++i) {
      if (line[columns[i]] != tuple[i]) {
        changedVars.push_back(tuple[i]);
      }
    }
    if (changedVars.size() == 0) {
      continue;
    }
    // check constraint, before tmpScore or after it?
    if (checkFunction == 1) {
      bool need_to_check = false;
      for (auto v : changedVars) {
        const Options &options = specificationFile.getOptions();
        if (option_constrained_indicator[options.option(v)]) {
          need_to_check = true;
          break;
        }
      }
      if (!validater.valida_row(array[lineIndex], changedVars)) {
        continue;
      }
    }
    else {
      InputKnown known;
      for (unsigned column = 0, passing = 0; column < line.size(); ++column) {
        if (passing < tuple.size() && column == columns[passing]) {
          known.append(InputTerm(false, tuple[passing++]));
        } else {
          known.append(InputTerm(false, line[column]));
        }
      }
      if (!satSolver(known)) {
        continue;
      }
    }
    // greedy
    long long tmpScore;
    if (scoringFunction == 1){
      if (changedVars.size() > 1) {
        tmpScore = multiVarScoreOfRow2(changedVars, lineIndex);
      } else {
        tmpScore = varScoreOfRow3(changedVars[0], lineIndex);
      }
    }
    else {  
      tmpScore = multiVarScoreOfRow(changedVars, lineIndex);
    }
    if (bestScore < tmpScore) {
      bestScore = tmpScore;
      bestRows.clear();
      bestRows.push_back(lineIndex);
    } else if (bestScore == tmpScore) {
      bestRows.push_back(lineIndex);
    }
  }
  // need to handle when bestRows.size() == 0
  if (bestRows.size() != 0) {
    unsigned lineIndex = bestRows[mersenne.next(bestRows.size())];
    changedVars.clear();
    for (unsigned i = 0; i < tuple.size(); ++i) {
      if (array[lineIndex][columns[i]] != tuple[i]) {
        changedVars.push_back(tuple[i]);
      }
    }
    multiVarReplace2(changedVars, lineIndex);
    return;
  }
  replaceRow2(mersenne.next(array.size()), tupleEncode);
}

void CoveringArray::tabugw() {
  unsigned base = mersenne.next(uncoveredTuples.size());
  std::vector<unsigned> firstBestRows;
  std::vector<unsigned> firstBestVars;
  std::vector<unsigned> bestRows;
  std::vector<unsigned> bestVars;
  long long bestScore = std::numeric_limits<long long>::min();
  for (size_t i = 0; i < uncoveredTuples.size(); ++i) {
    const unsigned tupleEncode =
        uncoveredTuples.encode((base + i) % uncoveredTuples.size());
    const std::vector<unsigned> &tuple = coverage.getTuple(tupleEncode);
    const std::vector<unsigned> &columns = coverage.getColumns(tupleEncode);
    for (unsigned lineIndex = 0; lineIndex < array.size(); ++lineIndex) {
      std::vector<unsigned> &line = array[lineIndex];
      unsigned diffCount = 0;
      unsigned diffVar;
      for (unsigned i = 0; i < tuple.size(); ++i) {
        if (line[columns[i]] != tuple[i]) {
          diffCount++;
          diffVar = tuple[i];
        }
      }
      if (diffCount > 1) {
        continue;
      }
      unsigned diffOption = specificationFile.getOptions().option(diffVar);
      // Tabu
      if (entryTabu.isTabu(Entry(lineIndex, diffOption))) {
        continue;
      }
      // my check
      if (!validater.valida_change(lineIndex, diffOption, line[diffOption],
                                   diffVar)) {
        continue;
      }
      long long tmpScore = varScoreOfRow3(diffVar, lineIndex);
      if (bestScore < tmpScore) {
        bestScore = tmpScore;
        if (i == 0) {
          firstBestRows.clear();
          firstBestRows.push_back(lineIndex);
          firstBestVars.clear();
          firstBestVars.push_back(diffVar);
        }
        bestRows.clear();
        bestRows.push_back(lineIndex);
        bestVars.clear();
        bestVars.push_back(diffVar);
      } else if (bestScore == tmpScore) {
        bestRows.push_back(lineIndex);
        bestVars.push_back(diffVar);
        if (i == 0) {
          firstBestRows.push_back(lineIndex);
          firstBestVars.push_back(diffVar);
        }
      }
    }
  }
  if (bestScore > 0) {
    unsigned ran = mersenne.next(bestRows.size());
    replace(bestVars[ran], bestRows[ran]);
    return;
  }

  if (mersenne.nextClosed() < 0.001) {
    replaceRow2(mersenne.next(array.size()), uncoveredTuples.encode(base));
    return;
  }
  if (firstBestRows.size() != 0) {
    unsigned ran = mersenne.next(firstBestRows.size());
    replace(firstBestVars[ran], firstBestRows[ran]);
    return;
  }

  const unsigned tupleEncode =
      uncoveredTuples.encode(base % uncoveredTuples.size());
  const std::vector<unsigned> &tuple = coverage.getTuple(tupleEncode);
  const std::vector<unsigned> &columns = coverage.getColumns(tupleEncode);

  bestRows.clear();

  std::vector<unsigned> changedVars;
  std::vector<unsigned> org_vars;
  for (unsigned lineIndex = 0; lineIndex < array.size(); ++lineIndex) {
    org_vars.clear();
    changedVars.clear();
    std::vector<unsigned> &line = array[lineIndex];
    for (unsigned i = 0; i < tuple.size(); ++i) {
      if (line[columns[i]] != tuple[i]) {
        org_vars.push_back(line[columns[i]]);
        changedVars.push_back(tuple[i]);
      }
    }
    if (changedVars.size() == 0) {
      continue;
    }
    // Tabu
    bool isTabu = false;
    for (auto v : changedVars) {
      unsigned diffOption = specificationFile.getOptions().option(v);
      if (entryTabu.isTabu(Entry(lineIndex, diffOption))) {
        isTabu = true;
        break;
      }
    }
    if (isTabu) {
      continue;
    }
    // check constraint, before tmpScore or after it?
    bool need_to_check = false;
    for (auto v : changedVars) {
      const Options &options = specificationFile.getOptions();
      if (option_constrained_indicator[options.option(v)]) {
        need_to_check = true;
        break;
      }
    }

    // my check
    if (need_to_check && !validater.valida_row(array[lineIndex], changedVars)) {
      continue;
    }
    // greedy
    long long tmpScore;
    if (changedVars.size() > 1) {
      tmpScore = multiVarScoreOfRow2(changedVars, lineIndex);
    } else {
      tmpScore = varScoreOfRow3(changedVars[0], lineIndex);
    }
    if (bestScore < tmpScore) {
      bestScore = tmpScore;
      bestRows.clear();
      bestRows.push_back(lineIndex);
    } else if (bestScore == tmpScore) {
      bestRows.push_back(lineIndex);
    }
  }
  // need to handle when bestRows.size() == 0
  if (bestRows.size() != 0) {
    // std::cout << "change_mutivar" << std::endl;
    unsigned lineIndex = bestRows[mersenne.next(bestRows.size())];
    changedVars.clear();
    for (unsigned i = 0; i < tuple.size(); ++i) {
      if (array[lineIndex][columns[i]] != tuple[i]) {
        changedVars.push_back(tuple[i]);
      }
    }
    if (changedVars.size() > 1) {
      // multiVarReplace(changedVars, lineIndex);
      for (auto v : changedVars) {
        replace(v, lineIndex);
      }
    } else {
      replace(changedVars[0], lineIndex);
    }
    return;
  }
  replaceRow2(mersenne.next(array.size()), tupleEncode);
}

int CoveringArray::checkInstance(const ConstraintFile &constraintFile) {
  int strength = specificationFile.getStrenth();
  int constraints = constraintFile.getClauses().size();
  if(strength <= 3) {
    if(constraints >= 128) {
      return 1;
    }
    else {
      return 0;
    }
  }
  else {
    if(constraints == 25 || constraints == 31 || constraints == 22 || constraints == 0 || constraints == 13 || constraints == 125) {
      return 0;
    }
    else {
      return 1;
    }
  }

}
