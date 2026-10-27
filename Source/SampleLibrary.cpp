#include "SampleLibrary.h"
#include <algorithm>
#include <cstring>
#include <map>
#include <set>

namespace kf
{
namespace
{
    juce::File appDataDir()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Forgeworks").getChildFile ("Kickforge");
    }

    // Wörter (klein) aus einem Text: Trennung an allem, was kein Buchstabe/Ziffer ist
    juce::StringArray words (const juce::String& s)
    {
        juce::StringArray w;
        juce::String cur;
        for (auto c : s.toLowerCase())
        {
            if (juce::CharacterFunctions::isLetterOrDigit (c)) cur += c;
            else if (cur.isNotEmpty()) { w.add (cur); cur.clear(); }
        }
        if (cur.isNotEmpty()) w.add (cur);
        return w;
    }

    bool hasAny (const juce::StringArray& w, std::initializer_list<const char*> keys)
    {
        for (auto* k : keys)
            if (w.contains (k)) return true;
        return false;
    }

    juce::String displayName (const juce::String& fileNameNoExt)
    {
        auto n = fileNameNoExt;
        for (auto* pre : { "TKNVLT_ITKB_", "TKNVLT_ITKVAULT_", "TKNVLT_ITDL4_", "TKNVLT_ITB_", "TKNVLT_" })
            if (n.startsWithIgnoreCase (pre)) { n = n.substring ((int) std::strlen (pre)); break; }
        return n.replaceCharacter ('_', ' ').trim();
    }

    // ---------------- Grundton-Erkennung (YIN auf tiefpassgefiltertem, dezimiertem Signal) -------------
    float yin (const float* x, int n, double sr, float fmin, float fmax, float& confidence)
    {
        const int tmin = (int) (sr / fmax), tmax = (int) (sr / fmin);
        const int w = n - tmax;
        confidence = 1.0f;
        if (w < tmax || tmin < 2) return 0.0f;
        std::vector<float> d ((size_t) tmax + 1, 0.0f), c ((size_t) tmax + 1, 1.0f);
        for (int t = 1; t <= tmax; ++t)
        {
            double s = 0.0;
            for (int i = 0; i < w; ++i) { const float e = x[i] - x[i + t]; s += e * e; }
            d[(size_t) t] = (float) s;
        }
        double cs = 0.0;
        for (int t = 1; t <= tmax; ++t)
        {
            cs += d[(size_t) t];
            c[(size_t) t] = cs > 1.0e-12 ? (float) (d[(size_t) t] * t / cs) : 1.0f;
        }
        int best = -1;
        for (int t = tmin; t < tmax; ++t)
            if (c[(size_t) t] < 0.2f) { best = t; break; }
        if (best < 0)
        {
            best = tmin;
            for (int t = tmin; t < tmax; ++t) if (c[(size_t) t] < c[(size_t) best]) best = t;
        }
        while (best + 1 < tmax && c[(size_t) best + 1] < c[(size_t) best]) ++best;
        double tf = best;
        if (best >= 1 && best < tmax - 1)
        {
            const double a = c[(size_t) best - 1], b = c[(size_t) best], cc = c[(size_t) best + 1];
            const double den = a - 2.0 * b + cc;
            if (std::abs (den) > 1.0e-12) tf += 0.5 * (a - cc) / den;
        }
        confidence = c[(size_t) best];
        return tf > 0.0 ? (float) (sr / tf) : 0.0f;
    }
}

// =========================================================================
SampleLibrary::SampleLibrary() : juce::Thread ("Kickforge Library")
{
    juce::PropertiesFile::Options o;
    o.applicationName     = "settings";
    o.filenameSuffix      = ".xml";
    o.folderName          = "Forgeworks" + juce::File::getSeparatorString() + "Kickforge";
    o.osxLibrarySubFolder = "Application Support";
    settings = std::make_unique<juce::PropertiesFile> (o);
    cacheFile = appDataDir().getChildFile ("library-cache.json");
    loadSettings();
    rescan();
}

SampleLibrary::~SampleLibrary()
{
    stopThread (5000);
}

void SampleLibrary::loadSettings()
{
    const juce::ScopedLock sl (lock);
    if (settings->containsKey ("libraryRoots"))
        roots = juce::StringArray::fromLines (settings->getValue ("libraryRoots"));
    else
        needsAutoDetect = true;   // erster Start: im Hintergrund nach Kick-Builder-Ordnern suchen
    roots.removeEmptyStrings();
    roots.removeDuplicates (true);
}

void SampleLibrary::saveSettings()
{
    const juce::ScopedLock sl (lock);
    settings->setValue ("libraryRoots", roots.joinIntoString ("\n"));
    settings->saveIfNeeded();
}

juce::StringArray SampleLibrary::getRoots() const { const juce::ScopedLock sl (lock); return roots; }

void SampleLibrary::setRoots (const juce::StringArray& r)
{
    {
        const juce::ScopedLock sl (lock);
        roots = r;
        roots.removeEmptyStrings();
        roots.removeDuplicates (true);
    }
    saveSettings();
    rescan();
}

void SampleLibrary::addRoot (const juce::File& dir)
{
    auto r = getRoots();
    r.addIfNotAlreadyThere (dir.getFullPathName());
    setRoots (r);
}

void SampleLibrary::rescan()
{
    stopThread (5000);
    scanning = true;
    startThread (juce::Thread::Priority::low);
}

juce::String SampleLibrary::getStatusText() const
{
    const juce::ScopedLock sl (lock);
    if (scanning.load()) return juce::String::fromUTF8 ("Bibliothek wird eingelesen\xe2\x80\xa6");
    if (roots.isEmpty()) return juce::String::fromUTF8 ("Keine Bibliothek \xe2\x80\x93 Ordner w\xc3\xa4hlen");
    if (entries.empty()) return "Keine passenden Samples gefunden";
    return juce::String ((int) entries.size()) + juce::String::fromUTF8 (" Samples \xc2\xb7 ") + juce::String ((int) kits.size()) + " Kits";
}

std::vector<LibEntry> SampleLibrary::getEntries() const { const juce::ScopedLock sl (lock); return entries; }
std::vector<LibKit>   SampleLibrary::getKits() const    { const juce::ScopedLock sl (lock); return kits; }

LibEntry SampleLibrary::getEntry (int index) const
{
    const juce::ScopedLock sl (lock);
    return juce::isPositiveAndBelow (index, (int) entries.size()) ? entries[(size_t) index] : LibEntry();
}

std::vector<int> SampleLibrary::entriesForLayer (int layer) const
{
    const juce::ScopedLock sl (lock);
    std::vector<int> idx;
    for (int i = 0; i < (int) entries.size(); ++i)
        if (entries[(size_t) i].layer == layer) idx.push_back (i);
    std::stable_sort (idx.begin(), idx.end(), [this] (int a, int b)
    {
        const auto& ea = entries[(size_t) a];
        const auto& eb = entries[(size_t) b];
        if (ea.fullKick != eb.fullKick) return ! ea.fullKick;           // Layer-Samples vor kompletten Kicks
        const int g = ea.group.compareNatural (eb.group);
        if (g != 0) return g < 0;
        return ea.display.compareNatural (eb.display) < 0;
    });
    return idx;
}

int SampleLibrary::indexOfFile (const juce::File& f) const
{
    const juce::ScopedLock sl (lock);
    for (int i = 0; i < (int) entries.size(); ++i)
        if (entries[(size_t) i].file == f) return i;
    return -1;
}

int SampleLibrary::findByFileName (const juce::String& fileName) const
{
    const juce::ScopedLock sl (lock);
    for (int i = 0; i < (int) entries.size(); ++i)
        if (entries[(size_t) i].file.getFileName().equalsIgnoreCase (fileName)) return i;
    return -1;
}

// =========================================================================
// Einordnung
// =========================================================================
bool SampleLibrary::classify (const juce::File& f, LibEntry& e)
{
    const auto nameWords = words (f.getFileNameWithoutExtension());
    juce::StringArray dirWords;
    for (auto d = f.getParentDirectory(); d.getParentDirectory() != d && dirWords.size() < 60; d = d.getParentDirectory())
        dirWords.addArray (words (d.getFileName()));
    juce::StringArray all (nameWords);
    all.addArray (dirWords);

    // Keine Kicks: Loops, Fills, FX, Hats, Templates …
    if (hasAny (all, { "loop", "loops", "fill", "fills", "buildup", "buildups", "riser", "risers", "sweep", "sweeps",
                       "sweepup", "sweepdown", "atmosphere", "atmospheres", "texture", "textures", "impact", "impacts",
                       "exhaust", "exhausts", "crash", "crashes", "hat", "hats", "ride", "rides", "clap", "claps",
                       "snare", "snares", "perc", "percussion", "template", "templates", "walkthrough", "fx", "predrop",
                       "vocal", "vocals", "synth", "bass", "project", "projects",
                       "imported" }))   // "Samples/Imported" = Kopien aus Ableton-Packs
        return false;

    e = LibEntry();
    e.file = f;
    e.display = displayName (f.getFileNameWithoutExtension());
    e.group = f.getParentDirectory().getFileName();

    auto pick = [&e] (int layer, const char* role) { e.layer = layer; e.role = role; return true; };
    bool ok = false;
    if      (hasAny (nameWords, { "click", "clicks" }))                       ok = pick (0, "CLICK");
    else if (hasAny (nameWords, { "top", "toplayer" }))                       ok = pick (3, "TOP");
    else if (hasAny (nameWords, { "punch", "punches" }))                      ok = pick (1, "PUNCH");
    else if (hasAny (nameWords, { "tail", "tails" }))                         ok = pick (2, "TAIL");
    else if (hasAny (nameWords, { "crunch", "crunches" }))                    ok = pick (2, "CRUNCH");
    else if (hasAny (nameWords, { "rumble", "rumbles" }))                     ok = pick (2, "RUMBLE");
    else if (hasAny (dirWords,  { "click", "clicks" }))                       ok = pick (0, "CLICK");
    else if (hasAny (dirWords,  { "top" }))                                   ok = pick (3, "TOP");
    else if (hasAny (dirWords,  { "punch", "punches" }))                      ok = pick (1, "PUNCH");
    else if (hasAny (dirWords,  { "crunch", "crunches", "rumble", "rumbles", "tail", "tails" }))
                                                                              ok = pick (2, "RUMBLE");
    else if (hasAny (all, { "kick", "kicks", "bassdrum", "bd" }))
    {
        ok = pick (1, "FULL");
        e.fullKick = true;
        e.group = juce::String::fromUTF8 ("Full Kicks \xc2\xb7 ") + e.group;
    }
    if (! ok) return false;

    // Kit-Erkennung: KICK_<n>_<ROLLE>  bzw.  HEAVY_RAW_<ROLLE>_<n>
    juce::StringArray tok;
    tok.addTokens (f.getFileNameWithoutExtension().toUpperCase(), "_ -", "");
    tok.removeEmptyStrings();
    for (int i = 0; i + 2 < tok.size(); ++i)
    {
        if (tok[i] == "KICK" && tok[i + 1].containsOnly ("0123456789")
            && juce::StringArray { "PUNCH", "RUMBLE", "TAIL", "CLICK", "TOP" }.contains (tok[i + 2]))
        {
            e.kit = "KICK " + juce::String (tok[i + 1].getIntValue());
            break;
        }
        if (tok[i] == "HEAVY" && tok[i + 1] == "RAW" && i + 3 < tok.size() && tok[i + 3].containsOnly ("0123456789"))
        {
            e.kit = "HEAVY RAW " + juce::String (tok[i + 3].getIntValue());
            break;
        }
    }
    return true;
}

float SampleLibrary::detectRootHz (const juce::AudioBuffer<float>& buf, double sr)
{
    const int n = buf.getNumSamples();
    if (n < (int) (0.2 * sr)) return 0.0f;

    // Mono, Tiefpass 180 Hz (4. Ordnung), dezimieren auf ~11 kHz
    std::vector<float> m ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        float s = 0.0f;
        for (int ch = 0; ch < buf.getNumChannels(); ++ch) s += buf.getSample (ch, i);
        m[(size_t) i] = s / (float) juce::jmax (1, buf.getNumChannels());
    }
    juce::IIRFilter f1, f2;
    f1.setCoefficients (juce::IIRCoefficients::makeLowPass (sr, 180.0, 0.541));
    f2.setCoefficients (juce::IIRCoefficients::makeLowPass (sr, 180.0, 1.307));
    f1.processSamples (m.data(), n);
    f2.processSamples (m.data(), n);
    const int D = juce::jmax (1, (int) std::round (sr / 11025.0));
    std::vector<float> d;
    for (int i = 0; i < n; i += D) d.push_back (m[(size_t) i]);
    const double srd = sr / D;

    std::vector<float> found;
    for (double a = 0.10; a < 0.33; a += 0.02)
    {
        const int s0 = (int) (a * srd), len = (int) (0.07 * srd);
        if (s0 + len > (int) d.size()) break;
        float pk = 0.0f, mean = 0.0f;
        for (int i = 0; i < len; ++i) { pk = juce::jmax (pk, std::abs (d[(size_t) (s0 + i)])); mean += d[(size_t) (s0 + i)]; }
        if (pk < 0.02f) continue;
        mean /= (float) len;
        std::vector<float> seg ((size_t) len);
        for (int i = 0; i < len; ++i) seg[(size_t) i] = d[(size_t) (s0 + i)] - mean;
        float conf = 1.0f;
        const float hz = yin (seg.data(), len, srd, 32.0f, 130.0f, conf);
        if (hz > 0.0f && conf < 0.35f) found.push_back (hz);
    }
    if (found.size() < 3) return 0.0f;
    std::sort (found.begin(), found.end());
    return found[found.size() / 2];
}

juce::StringArray SampleLibrary::autoDetectRoots()
{
    juce::StringArray result;
    const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    juce::Array<juce::File> level { home.getChildFile ("Downloads"),
                                    juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                                    juce::File::getSpecialLocation (juce::File::userDesktopDirectory),
                                    juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                    home.getChildFile ("Splice") };
    int visited = 0;
    for (int depth = 0; depth < 4 && ! level.isEmpty() && visited < 4000; ++depth)
    {
        juce::Array<juce::File> next;
        for (auto& dir : level)
        {
            if (! dir.isDirectory()) continue;
            for (auto& sub : dir.findChildFiles (juce::File::findDirectories, false))
            {
                if (++visited > 4000) break;
                const auto n = sub.getFileName().toLowerCase();
                if (n.contains ("kick builder") || n.contains ("kickbuilder") || n.contains ("kick vault"))
                    result.addIfNotAlreadyThere (sub.getFullPathName());
                else if (! n.startsWith ("."))
                    next.add (sub);
            }
        }
        level = next;
    }
    return result;
}

// =========================================================================
// Scan (Hintergrund-Thread)
// =========================================================================
void SampleLibrary::run()
{
    scanning = true;
    if (needsAutoDetect.exchange (false))
    {
        const auto found = autoDetectRoots();
        {
            const juce::ScopedLock sl (lock);
            roots = found;
        }
        saveSettings();
    }
    const auto rootsCopy = getRoots();

    // Cache: Pfad → { size, mtime, root }
    juce::DynamicObject::Ptr cacheOut = new juce::DynamicObject();
    const auto cacheIn = juce::JSON::parse (cacheFile);

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();

    std::vector<LibEntry> found;
    std::map<juce::String, size_t> byKey;   // Duplikate (gleicher Name + Größe) nur einmal
    for (auto& r : rootsCopy)
    {
        const juce::File dir (r);
        if (! dir.isDirectory()) continue;
        for (auto& f : dir.findChildFiles (juce::File::findFiles, true, "*.wav;*.aif;*.aiff;*.flac"))
        {
            if (threadShouldExit()) { scanning = false; return; }
            LibEntry e;
            if (! classify (f, e)) continue;
            const auto key = f.getFileName().toLowerCase() + ":" + juce::String (f.getSize());
            auto it = byKey.find (key);
            if (it != byKey.end())
            {
                // Original bevorzugen (nicht die Kopie aus einem "Imported"-Ordner)
                if (found[it->second].file.getFullPathName().containsIgnoreCase ("imported")
                    && ! f.getFullPathName().containsIgnoreCase ("imported"))
                    found[it->second] = e;
                continue;
            }
            byKey[key] = found.size();
            found.push_back (e);
        }
    }

    // Länge prüfen + Grundton (Punch/Rumble) – mit Cache
    std::vector<LibEntry> kept;
    for (auto& e : found)
    {
        if (threadShouldExit()) { scanning = false; return; }
        const auto path = e.file.getFullPathName();
        const auto size = e.file.getSize();
        const auto mtime = e.file.getLastModificationTime().toMilliseconds();
        const auto c = cacheIn.getProperty (path, juce::var());
        float root = -1.0f; double len = -1.0;
        if (c.isObject() && (juce::int64) c.getProperty ("size", 0) == size && (juce::int64) c.getProperty ("mtime", 0) == mtime)
        {
            root = (float) (double) c.getProperty ("root", 0.0);
            len  = (double) c.getProperty ("len", -1.0);
        }
        if (len < 0.0 || (root < 0.0f && (e.layer == 1 || e.layer == 2)))
        {
            std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (e.file));
            if (rd == nullptr || rd->sampleRate <= 0.0) continue;
            len = (double) rd->lengthInSamples / rd->sampleRate;
            root = 0.0f;
            if ((e.layer == 1 || e.layer == 2) && len <= 2.5)
            {
                const int n = (int) juce::jmin ((juce::int64) (0.45 * rd->sampleRate), rd->lengthInSamples);
                juce::AudioBuffer<float> b ((int) juce::jmax (1u, rd->numChannels), n);
                rd->read (&b, 0, n, 0, true, true);
                root = detectRootHz (b, rd->sampleRate);
            }
        }
        juce::DynamicObject::Ptr o = new juce::DynamicObject();
        o->setProperty ("size", size); o->setProperty ("mtime", mtime);
        o->setProperty ("root", root); o->setProperty ("len", len);
        cacheOut->setProperty (path, juce::var (o.get()));
        if (len < 0.005 || len > 2.5) continue;   // keine One-Shot-Kick
        e.rootHz = juce::jmax (0.0f, root);
        kept.push_back (e);
    }

    // Kits bilden
    std::map<juce::String, LibKit> kitMap;
    std::map<juce::String, std::map<juce::String, int>> roles;   // Kit → Rolle → Index
    for (int i = 0; i < (int) kept.size(); ++i)
        if (kept[(size_t) i].kit.isNotEmpty())
            roles[kept[(size_t) i].kit][kept[(size_t) i].role] = i;
    for (auto& [name, r] : roles)
    {
        LibKit k;
        k.name = name;
        auto get = [&r] (std::initializer_list<const char*> pref)
        {
            for (auto* p : pref) { auto it = r.find (p); if (it != r.end()) return it->second; }
            return -1;
        };
        k.entry[0] = get ({ "CLICK" });
        k.entry[1] = get ({ "PUNCH" });
        k.entry[2] = get ({ "TAIL", "CRUNCH", "RUMBLE" });
        k.entry[3] = get ({ "TOP" });
        // Kit-Grundton: aus dem (unbearbeiteten) Rumble am verlässlichsten
        for (auto* p : { "RUMBLE", "TAIL", "CRUNCH", "PUNCH" })
        {
            const int i = get ({ p });
            if (i >= 0 && kept[(size_t) i].rootHz > 0.0f) { k.rootHz = kept[(size_t) i].rootHz; break; }
        }
        if (k.entry[1] < 0 && k.entry[2] < 0) continue;
        for (auto& [role, i] : r)
            if (k.rootHz > 0.0f) kept[(size_t) i].rootHz = k.rootHz;   // ganzes Kit in einer Tonart
        kitMap[name] = k;
    }
    std::vector<LibKit> kitList;
    for (auto& [n, k] : kitMap) kitList.push_back (k);
    std::sort (kitList.begin(), kitList.end(), [] (const LibKit& a, const LibKit& b)
    {
        const bool ha = a.name.startsWith ("HEAVY"), hb = b.name.startsWith ("HEAVY");
        if (ha != hb) return ! ha;
        return a.name.compareNatural (b.name) < 0;
    });

    if (! cacheFile.getParentDirectory().exists()) cacheFile.getParentDirectory().createDirectory();
    cacheFile.replaceWithText (juce::JSON::toString (juce::var (cacheOut.get()), true));

    {
        const juce::ScopedLock sl (lock);
        entries = std::move (kept);
        kits = std::move (kitList);
    }
    version.fetch_add (1);
    scanning = false;
}

} // namespace kf
