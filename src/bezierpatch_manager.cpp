#include "bezierpatch_manager.h"

BezierPatch_Manager::BezierPatch_Manager(GLint vaoId, GLint vboPositionId, GLint eboId, GLint uColorLocation) :
    m_patchs(),
    m_dependencies(),
    m_VAOId(vaoId),
    m_VBOPositionId(vboPositionId),
    m_EBOId(eboId),
    m_uColorLocation(uColorLocation),
    m_VBOPositionSize(0),
    m_EBOSize(0),
    m_currentBaseVertex(0),
    m_VBOCapacity(0),
    m_EBOCapacity(0),
    m_selectionHitProperties()
{
    BezierPatch::setVBOPosition(m_VBOPositionId);
    BezierPatch::setEBO(m_EBOId);
}

void BezierPatch_Manager::append(BezierPatch* patch, bool reallocate)
{
    if(patch!=NULL)
    {
        m_patchs.insert(std::pair<unsigned int, BezierPatch*>(patch->id(), patch));

        //the indexes where we finished writting
        GLsizeiptr firstVBO=m_VBOPositionSize;
        GLsizeiptr firstEBO=m_EBOSize;
        GLsizeiptr baseVertex=m_currentBaseVertex;

        m_VBOPositionSize   +=      patch->getSizeVBOPosition()+patch->getSizeVBOSurface();
        m_EBOSize           +=      patch->getSizeEBOPosition()+patch->getSizeEBOSurface();
        m_currentBaseVertex +=      patch->getNumberOfPointsPatch()+patch->getNumberOfPointsSurface();

        if(reallocate && (m_VBOCapacity<m_VBOPositionSize || m_EBOCapacity<m_EBOSize))
        {
            //remake the whole scene
            remakeScene();
        }
        else
        {
            //set first indexes and base vertex only
            patch->setFirstVBO(firstVBO);
            patch->setFirstEBO(firstEBO);
            patch->setBaseVertexEBO(baseVertex);
        }
        m_selectedCPDependency = m_dependencies.end();
    }
}

BezierPatch *BezierPatch_Manager::remove(unsigned int id)
{
    iterator position=m_patchs.find(id);
    if(position!=m_patchs.end())
    {
        BezierPatch *removedPtr=(*position).second;
        m_patchs.erase(position);
        remakeScene();

        std::map<unsigned int, PatchDependencySolver>::iterator dependencyPosition = m_dependencies.find(id);
        if(dependencyPosition!=m_dependencies.end())
        {
            m_dependencies.erase(dependencyPosition);
        }
        m_selectedCPDependency = m_dependencies.end();
        return removedPtr;
    }
    else
        WARNING("BezierPatch_Manager - remove: id wasn't found");
    return NULL;
}

//dependencies

void BezierPatch_Manager::removeDependency(unsigned int id)
{
    std::map<unsigned int, PatchDependencySolver>::iterator it=m_dependencies.find(id);
    if(it!=m_dependencies.end())
    {
        m_dependencies.erase(it);
    }
}

PatchDependencySolver &BezierPatch_Manager::addDependency(unsigned int id)
{
     std::map<unsigned int, PatchDependencySolver>::iterator it
             =m_dependencies.insert(std::pair<unsigned int, PatchDependencySolver>(id, PatchDependencySolver(*this))).first;
     return (*it).second;
}

void BezierPatch_Manager::updateDependency(unsigned int id)
{
    try
    {
        m_dependencies.at(id).updateDependencies();
    }
    catch(std::out_of_range e)
    {
        WARNING("BezierPatch_Manager - updateDependency: dependency wasn't found");
    }
}

void BezierPatch_Manager::allocateScene()
{
    allocateVBOPosition();
    allocateEBO();
}

void BezierPatch_Manager::remakeScene()
{
    //first check if the desired size of the scene is bigger than its current maximum size and update first indexes
    m_VBOPositionSize   =   0;
    m_EBOSize           =   0;
    m_currentBaseVertex =   0;
    for(iterator it=begin(); it!=end(); ++it)
    {
        BezierPatch* patch=(*it).second;

        if(patch!=NULL)
        {
            //the patches need to be forced to re-fill datas into the VBO/EBO, as intended
            patch->notifyPatchChanged();

            patch->setFirstVBO(m_VBOPositionSize);
            patch->setFirstEBO(m_EBOSize);
            patch->setBaseVertexEBO(m_currentBaseVertex);

            m_VBOPositionSize   +=      patch->getSizeVBOPosition()+patch->getSizeVBOSurface();
            m_EBOSize           +=      patch->getSizeEBOPosition()+patch->getSizeEBOSurface();
            m_currentBaseVertex +=      patch->getNumberOfPointsPatch()+patch->getNumberOfPointsSurface();
        }
    }

    //re-allocate if the new size of the scene is bigger than the old size
    if(m_VBOPositionSize>m_VBOCapacity || m_VBOCapacity > 2*m_VBOPositionSize)
    {
        allocateVBOPosition();
    }

    //do that for the EBO, too
    if(m_EBOSize>m_EBOCapacity || m_EBOCapacity > 2*m_EBOSize)
    {
        allocateEBO();
    }

    //finally, let classes fill whatever information they want in their VBOs or EBOs.
    updateScene();
}

void BezierPatch_Manager::updateScene()
{
    //transfer informations to the VBO and the EBO.
    for(iterator it=begin(); it!=end(); ++it)
    {
        BezierPatch* patch=(*it).second;

        if(patch!=NULL)
        {
            patch->updateVBOPatch();
            patch->updateVBOSurface();
        }
    }
}

void BezierPatch_Manager::drawScene()
{
    for(iterator it=begin(); it!=end(); ++it)
    {
        BezierPatch* patch=(*it).second;

        patch->draw(m_uColorLocation);
    }
}

//CONTROL POINT SELECTION

bool BezierPatch_Manager::rayIntersectsCP(const glm::vec3& origin, const glm::vec3& direction, float r, float &distance)
{
    BezierPatch::RayHit hitProperties(r);

    for(iterator it=begin(); it!=end(); ++it)
    {
        BezierPatch* patch=(*it).second;

        //if the user can see these CPs
        if(patch->isDrawCP())
        {
            //try to connect with a control point (this function also modifies r)
            patch->rayIntersectsCP(origin, direction, hitProperties);
        }
    }
    if(hitProperties.occuredHit)
    {
        m_selectionHitProperties=hitProperties;
        distance=hitProperties.distanceHit;
        m_selectedCPDependency = m_dependencies.find(hitProperties.objectHit->id());
    }
    return hitProperties.occuredHit;
}

void BezierPatch_Manager::updateSelectedCP(const glm::vec3& newPosition)
{
    if(m_selectionHitProperties.objectHit!=NULL)
    {
        glm::vec3 oldCP=m_selectionHitProperties.objectHit->getPoint(m_selectionHitProperties.indexCPHit);
        m_selectionHitProperties.objectHit->setPoint(m_selectionHitProperties.indexCPHit, newPosition, true);
        if(m_selectedCPDependency != m_dependencies.end())
        {//there is a current dependency with the selected object
            bool needUpdate = true;
            glm::vec3 deltaCP=newPosition-oldCP;
            std::pair<size_t, size_t> coordinates;
            BezierPatch_Hexaedron *hexaedron=dynamic_cast<BezierPatch_Hexaedron*>(m_selectionHitProperties.objectHit);
            //fill coordinates
            if(hexaedron!=NULL)
            {
                //we need to project on the face of the dependency the current coordinates to get those of the dependency matrix
                needUpdate=hexaedron->projectOnFace(m_selectionHitProperties.firstCoordinate, m_selectionHitProperties.secondCoordinate, m_selectionHitProperties.thirdCoordinate,
                                            coordinates, (*m_selectedCPDependency).second.infos());
            }
            else
            {   //rectangle
                //projection = identity
                coordinates.first=m_selectionHitProperties.firstCoordinate;
                coordinates.second=m_selectionHitProperties.secondCoordinate;

            }
            //update the dependency by the difference between the old and new CP
            if(needUpdate)
                (*m_selectedCPDependency).second.updateDependency(coordinates.first, coordinates.second, deltaCP);
        }
    }
    else
        WARNING("BezierPatch_Manager : updateSelectedCP called with no selected CP");
}

void BezierPatch_Manager::releaseSelectedCP()
{
    m_selectionHitProperties.objectHit=NULL;
}

BezierPatch *BezierPatch_Manager::operator[](unsigned int i)
{
    return m_patchs[i];
}

void BezierPatch_Manager::setPatch(unsigned int index, BezierPatch* patch)
{
    m_patchs.at(index)=patch;
}

BezierPatch* BezierPatch_Manager::getPatch(unsigned int index)
{
    BezierPatch *foundPatch=NULL;
    try
    {
        foundPatch=m_patchs.at(index);
    }
    catch(std::out_of_range e)
    {
        WARNING("BezierPatch_Manager - getPatch: patch doesn't exist anymore");
    }
    return foundPatch;
}

//////////////////////PRIVATE////////////////////////////

void BezierPatch_Manager::allocateVBOPosition()
{
    //simple memory allocation
    glBindBuffer(GL_ARRAY_BUFFER, m_VBOPositionId);
    glBufferData(GL_ARRAY_BUFFER, m_VBOPositionSize, NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    m_VBOCapacity=m_VBOPositionSize;
}

void BezierPatch_Manager::allocateEBO()
{
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBOId);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_EBOSize, NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    m_EBOCapacity=m_EBOSize;
}
