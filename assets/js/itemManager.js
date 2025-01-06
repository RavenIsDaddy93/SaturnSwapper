window.saturn.itemManager = {
    currentId: "",
    addItem: function(name, id){
        const templateString = `<div class="container-item" onclick="saturn.itemManager.onClick('${id}')"><img src="https://fortnite-api.com/images/cosmetics/br/${id}/smallicon.png"/><h2>${name}</h2></div>`
        const container = document.getElementById('container');

        container.innerHTML += templateString;
    },
    clearItems: function() {
        var container = document.getElementById('container');
        container.innerHTML = "";
    },
    onClick: function(id) {
        this.currentId = id;
        if (OnIsItemConverted()) {
            saturn.modalManager.showModal('revert');
        }
        else {
            saturn.modalManager.showModal('apply');
        }
    },
    addLoadoutItem: function() {
        OnAddItem(this.currentId);
        saturn.modalManager.hideModal('apply');
        saturn.modalManager.hideModal('revert');
    },
    onSearch: function(event) {
        if (event.key == "Enter") {
            const search = document.getElementById('search').value;
            OnSearch(search);
            OnDisplayItems();
        }
    }
}