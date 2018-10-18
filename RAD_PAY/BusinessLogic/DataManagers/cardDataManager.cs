using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class cardDataManager
    {
        //cardViewModel
        //public long       card_id         { get; set; }
        //public string     number          { get; set; }
        //public string     expire          { get; set; }
        //public long?      uid             { get; set; }
        //public int?       is_primary      { get; set; }
        //public string     name            { get; set; }
        //public int?       tpl             { get; set; }
        //public long?      tr_limit        { get; set; }
        //public int?       block           { get; set; }
        //public int?       user_block      { get; set; }
        //public string     pc_token        { get; set; }
        //public string     owner           { get; set; }
        //public int?       foreign_card    { get; set; }
        //public string     owner_phone     { get; set; }
        //public long       daily_limit     { get; set; }

        public static void Add(cardViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new card
            {
                card_id      = model.card_id        ,   
                number       = model.number         ,
                expire       = model.expire         ,
                uid          = model.uid            ,
                is_primary   = model.is_primary     ,
                name         = model.name           ,
                tpl          = model.tpl            ,
                tr_limit     = model.tr_limit       ,
                block        = model.block          ,
                user_block   = model.user_block     ,
                pc_token     = model.pc_token       ,
                owner        = model.owner          ,
                foreign_card = model.foreign_card   ,
                owner_phone  = model.owner_phone    ,
                daily_limit  = model.daily_limit    ,
            };

            db.cards.Add(dbmodel);
        }

        public static void Modify(cardViewModel model, RAD_PAYEntities db)
        {
            var result = db.cards.Where(z => z.card_id == model.card_id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.card_id = model.card_id            ;   
                    dbmodel.number = model.number              ;
                    dbmodel.expire = model.expire              ;
                    dbmodel.uid = model.uid                    ;
                    dbmodel.is_primary = model.is_primary      ;
                    dbmodel.name = model.name                  ;
                    dbmodel.tpl = model.tpl                    ;
                    dbmodel.tr_limit = model.tr_limit          ;
                    dbmodel.block = model.block                ;
                    dbmodel.user_block = model.user_block      ;
                    dbmodel.pc_token = model.pc_token          ;
                    dbmodel.owner = model.owner                ;
                    dbmodel.foreign_card = model.foreign_card  ;
                    dbmodel.owner_phone = model.owner_phone    ;
                    dbmodel.daily_limit = model.daily_limit;
                }
            }
        }

        public static void Delete(cardViewModel model, RAD_PAYEntities db)
        {
            var result = db.cards.Where(z => z.card_id == model.card_id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.cards.Remove(dbmodel);
                }
            }
        }

        public static List<cardViewModel> Get(cardViewModel model, RAD_PAYEntities db)
        {
            List<cardViewModel> list = null;

            var query = from resmodel in db.cards
                        select new cardViewModel
                        {
                            card_id = resmodel.card_id,
                            number = resmodel.number,
                            expire = resmodel.expire,
                            uid = resmodel.uid,
                            is_primary = resmodel.is_primary,
                            name = resmodel.name,
                            tpl = resmodel.tpl,
                            tr_limit = resmodel.tr_limit,
                            block = resmodel.block,
                            user_block = resmodel.user_block,
                            pc_token = resmodel.pc_token,
                            owner = resmodel.owner,
                            foreign_card = resmodel.foreign_card,
                            owner_phone = resmodel.owner_phone,
                            daily_limit = resmodel.daily_limit,
                        };

            list = query.ToList();

            return list;
        }
    }
}