/* -*-c++-*- */
/* osgEarth - Geospatial SDK for OpenSceneGraph
* Copyright 2020 Pelican Mapping
* http://osgearth.org
*
* osgEarth is free software; you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>
*/
#include "Biome"

using namespace osgEarth;
using namespace osgEarth::Procedural;

#undef LC
#define LC "[Biome] "

std::vector<std::string>
AssetTraits::getPermutationStrings(const std::vector<std::string>& input)
{
    std::vector<std::string> sorted = input;
    std::sort(sorted.begin(), sorted.end());

    std::vector<std::string> result;

    std::ostringstream buf;

    for (int first = 0; first < sorted.size(); ++first)
    {
        for(int last = first; last < sorted.size(); ++last)
        {
            buf.str("");
            for (int i = first; i <= last; ++i)
            {
                if (i > first) buf << ",";
                buf << sorted[i];
            }
            result.push_back(buf.str());
        }
    }

    return std::move(result);
}

std::vector<std::vector<std::string>>
AssetTraits::getPermutationVectors(const std::vector<std::string>& input)
{
    std::vector<std::string> sorted = input;
    std::sort(sorted.begin(), sorted.end());

    std::vector<std::vector<std::string>> result;

    for (int first = 0; first < sorted.size(); ++first)
    {
        for (int last = first; last < sorted.size(); ++last)
        {
            std::vector<std::string> temp;
            for (int i = first; i <= last; ++i)
            {
                temp.emplace_back(sorted[i]);
            }
            result.emplace_back(std::move(temp));
        }
    }

    return std::move(result);
}

std::string
AssetTraits::toString(const std::vector<std::string>& traits)
{
    std::string result;
    if (!traits.empty()) {
        result = traits[0];
        for (int i = 1; i < traits.size(); ++i)
        {
            result += ',' + traits[i];
        }
    }
    return result;
}

ModelAsset::ModelAsset(const Config& conf)
{
    scale().setDefault(1.0f);
    stiffness().setDefault(0.5f);
    minLush().setDefault(0.0f);
    maxLush().setDefault(1.0f);
    sizeVariation().setDefault(0.0f);
    width().setDefault(0.0f);
    height().setDefault(0.0f);
    topBillboardHeight().setDefault(0.0f);
    traitsRequired().setDefault(false);

    conf.get("url", modelURI());
    conf.get("name", name());
    conf.get("side_url", sideBillboardURI());
    conf.get("top_url", topBillboardURI());
    conf.get("width", width());
    conf.get("height", height());
    conf.get("scale", scale());
    conf.get("size_variation", sizeVariation());
    conf.get("stiffness", stiffness());
    conf.get("min_lush", minLush());
    conf.get("max_lush", maxLush());
    conf.get("top_height", topBillboardHeight());
    conf.get("traits", traits());
    conf.get("traits_required", traitsRequired());

    // save the original so the user can extract user-defined values
    _sourceConfig = conf;
}

Config
ModelAsset::getConfig() const
{
    Config conf("model");
    conf.set("name", name());
    conf.set("url", modelURI());
    conf.set("side_url", sideBillboardURI());
    conf.set("top_url", topBillboardURI());
    conf.set("width", width());
    conf.set("height", height());
    conf.set("scale", scale());
    conf.set("size_variation", sizeVariation());
    conf.set("stiffness", stiffness());
    conf.set("min_lush", minLush());
    conf.set("max_lush", maxLush());
    conf.set("top_height", topBillboardHeight());
    conf.set("traits", traits());
    conf.set("traits_required", traitsRequired());
    return conf;
}

//...................................................................

MaterialAsset::MaterialAsset(const Config& conf)
{
    conf.get("name", name());
    conf.get("url", uri());
    conf.get("size", size());
}

Config
MaterialAsset::getConfig() const
{
    Config conf("asset");
    conf.set("name", name());
    conf.set("url", uri());
    conf.set("size", size());
    return conf;
}

//...................................................................

AssetCatalog::AssetCatalog(const Config& conf)
{
    // load all model asset defs
    ConfigSet modelassetgroups = conf.child("models").children("group");
    if (modelassetgroups.empty()) modelassetgroups = conf.child("modelassets").children("group");
    for (const auto& c : modelassetgroups)
    {
        std::string group = c.value("name");
        ConfigSet modelassets = c.children("asset");
        for (const auto& m : modelassets)
        {
            ModelAsset asset(m);
            asset.group() = group;
            _models[asset.name()] = asset;
        }
    }
    // load in all materials first into a temporary table
    std::map<std::string, MaterialAsset> temp_materials; // use map since we're iterating
    ConfigSet materialassets = conf.child("materials").children("asset");
    for (const auto& c : materialassets)
    {
        MaterialAsset a(c);
        if (a.name().isSet())
            temp_materials[a.name().get()] = std::move(a);
    }

    // make a list of materials, with the lifemap matrix materials first
    // so their indices are 'well known' from here on out.
    std::unordered_set<std::string> added;

    // add substrate textures:
    ConfigSet substrates = conf.child("lifemapmatrix").child("substrate").children("asset");
    for (const auto& c : substrates)
    {
        auto iter = temp_materials.find(c.value("name"));
        if (iter != temp_materials.end())
        {
            if (added.find(iter->first) == added.end())
            {
                _materials.push_back(iter->second);
                added.emplace(iter->first);
            }
            else
            {
                OE_WARN << LC << "LifeMapMatrix materials must all be unique!" << std::endl;
            }
        }
        else
        {
            OE_WARN << LC << "Unrecognized material asset \"" << iter->second.name().get() << "\" referenced in the lifemap matrix"
                << std::endl;
        }
    }

    // append overlay textures:
    ConfigSet overlays = conf.child("lifemapmatrix").child("overlay").children("asset");
    for (const auto& c : overlays)
    {
        auto iter = temp_materials.find(c.value("name"));
        if (iter != temp_materials.end())
        {
            if (added.find(iter->first) == added.end())
            {
                _materials.push_back(iter->second);
                added.emplace(iter->first);
            }
            else
            {
                OE_WARN << LC << "LifeMapMatrix materials must all be unique!" << std::endl;
            }
        }
        else
        {
            OE_WARN << LC << "Unrecognized material asset \"" << iter->second.name().get() << "\" referenced in the lifemap matrix" << std::endl;
        }
    }

    _lifemapMatrixWidth = _materials.size() / 2;

    // load all the other textures at the end, avoiding dupes
    for (auto& iter : temp_materials)
    {
        if (added.find(iter.first) == added.end())
        {
            _materials.push_back(iter.second);
            added.emplace(iter.first);
        }
    }
}

Config
AssetCatalog::getConfig() const
{
    Config conf("AssetCatalog");

    Config models("Models");
    for (auto& model : _models)
        models.add(model.second.getConfig());
    if (!models.empty())
        conf.add(models);

    return conf;
}

unsigned
AssetCatalog::getLifeMapMatrixWidth() const
{
    return _lifemapMatrixWidth;
}

const ModelAsset*
AssetCatalog::getModel(const std::string& name) const
{
    auto i = _models.find(name);
    return i != _models.end() ? &i->second : nullptr;
}

const MaterialAsset*
AssetCatalog::getMaterial(const std::string& name) const
{
    for (auto& material : _materials)
    {
        if (material.name() == name)
            return &material;
    }
    return nullptr;
}

bool
AssetCatalog::empty() const
{
    return _models.empty() && _materials.empty();
}

//...................................................................

Biome::Biome() :
    _index(-1),
    _parentBiome(nullptr),
    _implicit(false)
{
    //nop
}

Biome::Biome(const Config& conf, AssetCatalog* assetCatalog) :
    _index(-1),
    _parentBiome(nullptr),
    _implicit(false)
{
    conf.get("id", id());
    conf.get("name", name());
    conf.get("parent", parentId());
    conf.get("inherits_from", parentId());

    ConfigSet assets = conf.child("assets").children("asset");
    for (const auto& child : assets)
    {
        ModelAssetRef::Ptr m = std::make_shared<ModelAssetRef>();
        m->asset() = assetCatalog->getModel(child.value("name"));
        if (m->asset())
        {
            m->weight() = 1.0f;
            child.get("weight", m->weight());
            m->coverage() = 1.0f;
            child.get("fill", m->coverage()); // backwards compat
            child.get("coverage", m->coverage());

            _assetsToUse.emplace_back(m);
        }
    }
}

Config
Biome::getConfig() const
{
    Config conf("biome");
    conf.set("id", id());
    conf.set("name", name());
    conf.set("parent", parentId());

    //TODO
    OE_WARN << __func__ << " not implemented" << std::endl;
    // when we do implement this, take care to skip writing the asset pointers
    // when the parent biome is set.

    return conf;
}

Biome::ModelAssetRefs
Biome::getModelAssets(const std::string& group) const
{   
    ModelAssetRefs result;
    for (auto& ref : _assetsToUse)
    {
        if (ref->asset()->group() == group)
        {
            result.emplace_back(ref);
        }
    }
    return std::move(result);
}

bool
Biome::empty() const
{
    return _assetsToUse.empty();
}

Biome::ModelAssetRef::ModelAssetRef()
{
    weight() = 1.0f;
    coverage() = 1.0f;
    asset() = nullptr;
}

//..........................................................

BiomeCatalog::BiomeCatalog(const Config& conf) :
    _biomeIndexGenerator(1) // start at 1; 0 means "undefined"
{
    _assets = AssetCatalog(conf.child("assetcatalog"));

    ConfigSet biome_defs = conf.child("biomedefinitions").children("biome");
    if (biome_defs.empty())
        biome_defs = conf.child("biomecollection").children("biome"); // backwards compat
    if (biome_defs.empty())
        biome_defs = conf.child("biomes").children("biome"); // backwards compat

    // bring in all the raw biome definitions.
    for (const auto& b_conf : biome_defs)
    {
        Biome biome(b_conf, &_assets);

        biome._index = _biomeIndexGenerator++;

        // save these before calling std::move
        std::string id = biome.id();
        int index = biome.index();

        _biomes_by_index[index] = std::move(biome);
        _biomes_by_id[id] = &_biomes_by_index[index];
    }

    // resolve the explicit parent biome pointers.
    for (auto& index_and_biome : _biomes_by_index)
    {
        Biome& biome = index_and_biome.second;
        if (biome.parentId().isSet())
        {
            Biome* parent = getBiome(biome.parentId().get());
            if (parent)
            {
                biome._parentBiome = parent;
            }
        }
    }

    // check for cyclical parent relationships, which are illegal
    for (auto& index_and_biome : _biomes_by_index)
    {
        Biome& biome = index_and_biome.second;
        std::vector<const Biome*> visited;
        visited.push_back(&biome);

        const Biome* parent = biome._parentBiome;
        while (parent != nullptr)
        {
            if (std::find(visited.begin(), visited.end(), parent) != visited.end())
            {
                std::ostringstream buf;
                for (auto& bptr : visited)
                    buf << bptr->id() << " -> ";
                buf << parent->id();
                OE_WARN << LC << "***** I detected a parent loop in the biome catalog: " << buf.str() << std::endl;
                biome._parentBiome = nullptr;
                break;
            }
            else
            {
                visited.push_back(parent);
                parent = parent->_parentBiome;
            }
        }
    }

    // Scan the biomes for any assets with traits, and add a
    // new biome for every possible permutation :(

    // first collect every possible trait from the database into a unique, ordered set.
    std::set<std::string> traits_set;
    for (auto& index_and_biome : _biomes_by_index)
    {
        Biome& biome = index_and_biome.second;
        for (auto& asset_ref : biome._assetsToUse)
        {
            for (auto& trait : asset_ref->asset()->traits())
            {
                traits_set.emplace(trait);
            }
        }
    }

    // now copy that set into a (sorted) vector.
    std::vector<std::string> traits_vector;
    traits_vector.insert(traits_vector.end(), traits_set.begin(), traits_set.end());

    // Now get all possible trait permutations across the whole database.
    // It's a vector of vectors of strings, for example:
    // input:  ["A", "B", "C"]
    // output: [ ["A"], ["A", "B"], ["A", "B", "C"], ["B"], ["B", "C"], ["C"] ]
    auto all_permutations = AssetTraits::getPermutationVectors(traits_vector);

    // now, go through every biome and find any asset with a trait. 
    // Create an implicit biome for every permutation containing that trait.
    for (auto& index_and_biome : _biomes_by_index)
    {
        Biome& biome = index_and_biome.second;

        // skip implicit biomes since we already processed them
        if (biome._implicit)
            continue;

        // Collect all assets with traits so we can make a biome for each.
        // traverse "up" through the parent biomes to find them all.
        std::map<
            std::string,
            std::set<Biome::ModelAssetRef::Ptr>
        > lookup_table;

        for (const Biome* biome_ptr = &biome;
            biome_ptr != nullptr;
            biome_ptr = biome_ptr->_parentBiome)
        {
            for (auto& asset_ref : biome_ptr->_assetsToUse)
            {
                for (auto& asset_trait : asset_ref->asset()->traits())
                {
                    // find each global permuatation containing this asset trait
                    for (auto& perm : all_permutations)
                    {
                        if (std::find(perm.begin(), perm.end(), asset_trait) != perm.end())
                        {
                            // convert to a string (example: ["A", "B"] -> "A, B")
                            // and store in the lutter.
                            std::string perm_str = AssetTraits::toString(perm);
                            lookup_table[perm_str].emplace(asset_ref);
                        }
                    }
                }
            }
        }

        // for each possible combintation traits found, make a new
        // "implicit" biome and copy all the corresponding assets into it.
        for (auto& iter : lookup_table)
        {
            const std::string& perm_str = iter.first;
            auto& perm_assets = iter.second;

            // create a new biome for this traits permutation if it doesn't already exist:
            std::string sub_biome_id = biome.id() + "." + perm_str;
            const Biome* sub_biome = getBiome(sub_biome_id);
            if (sub_biome == nullptr)
            {
                // lastly, insert it into the index.
                int new_index = _biomeIndexGenerator++;
                Biome& new_biome = _biomes_by_index[new_index];
                new_biome._index = new_index;

                _biomes_by_id[sub_biome_id] = &new_biome;
                new_biome.id() = sub_biome_id;

                new_biome.name() = biome.name().get() + " (" + perm_str + ")";

                // marks this biome as one that was derived from traits data,
                // not defined by the configuration.
                new_biome._implicit = true;

                // By default the parent is unset, but this may change in the
                // search block that follows. An implicit biome without a 
                // similarly trait-ed parent should render nothing.
                new_biome.parentId().clear();

                // serach "up the chain" to see if there's a parent that can 
                // support fallback for this particular trait
                const Biome* ptr = &biome;
                while (ptr)
                {
                    // find the natural parent; if none, bail.
                    const Biome* parent = getBiome(ptr->parentId().get());
                    if (parent)
                    {
                        // find a traits-variation of the natural parent. If found, success.
                        std::string parent_with_traits = parent->id() + "." + perm_str;
                        Biome* temp = getBiome(parent_with_traits);
                        if (temp)
                        {
                            new_biome.parentId() = parent_with_traits;
                            new_biome._parentBiome = temp;
                            break;
                        }
                    }

                    // if not found, parent up and try again.
                    ptr = parent;
                }

                // copy over asset pointers for each asset matching the trait permutation
                for (auto asset_ptr : perm_assets)
                {
                    new_biome._assetsToUse.emplace_back(asset_ptr);
                }

                // sort the assets list by asset name so it is deterministic!
                std::sort(
                    new_biome._assetsToUse.begin(),
                    new_biome._assetsToUse.end(),
                    [](const Biome::ModelAssetRef::Ptr& lhs, const Biome::ModelAssetRef::Ptr& rhs) {
                        return lhs->asset()->name() < rhs->asset()->name();
                    });
            }
        }
    }

    // Now, for any asset marked as "traits exclusive", remove it from
    // all non-implicit biomes. Those should only exist in the impllicit
    // biomes that we created based on the trait data. This will prevent
    // those assets from being selected in a biome NOT associated with
    // the particular trait.
    for (auto& index_and_biome : _biomes_by_index)
    {
        Biome& biome = index_and_biome.second;

        // only care about explicit biomes; the implicit ones are already
        // correct
        if (!biome._implicit)
        {
            Biome::ModelAssetRefs temp;
            for (auto& asset_ref : biome._assetsToUse)
            {
                if (asset_ref->asset()->traitsRequired() == false)
                {
                    temp.emplace_back(asset_ref);
                }
            }
            biome._assetsToUse.swap(temp);
        }
    }

    // Finally, resolve each biome's asset list by traversing the parent biomes
    // to fill in missing data where necessary. Using a lambda since this is 
    // a recursive operation.
    std::function<void(Biome*)> resolveAssets = [&resolveAssets](Biome* biome)
    {
        if (biome->empty() && biome->_parentBiome != nullptr)
        {
            // make sure the parent is resovled first (recursive)
            resolveAssets(biome->_parentBiome);

            // copy all asset references over to this biome.
            biome->_assetsToUse = biome->_parentBiome->_assetsToUse;
        }
    };

    for (auto iter : _biomes_by_id)
    {
        resolveAssets(iter.second);
    }
}

Config
BiomeCatalog::getConfig() const
{
    Config conf;
    //TODO
    OE_WARN << __func__ << " not implemented" << std::endl;
    return conf;
}

const Biome*
BiomeCatalog::getBiomeByIndex(int index) const
{
    const auto i = _biomes_by_index.find(index);
    return i != _biomes_by_index.end() ? &i->second : nullptr;
}

const Biome*
BiomeCatalog::getBiome(const std::string& id) const
{
    const auto i = _biomes_by_id.find(id);
    return i != _biomes_by_id.end() ? i->second : nullptr;    
}

Biome*
BiomeCatalog::getBiome(const std::string& id)
{
    const auto i = _biomes_by_id.find(id);
    return i != _biomes_by_id.end() ? i->second : nullptr;
}

std::vector<const Biome*>
BiomeCatalog::getBiomes() const
{
    std::vector<const Biome*> result;
    for (auto& iter : _biomes_by_index)
        result.push_back(&iter.second);
    return std::move(result);
}

const AssetCatalog&
BiomeCatalog::getAssets() const
{
    return _assets;
}
